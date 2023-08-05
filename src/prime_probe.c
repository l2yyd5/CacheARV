#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aes.h>
#include <prime_probe.h>

#define LIMIT 400
#define SCALE 1000

typedef struct p_probe* p_probe_t;

struct p_probe {
    // setup data
    int blockSize;
    uint8_t fixMask[ST_BLOCKBYTES];
    uint8_t fixData[ST_BLOCKBYTES];

    // exec data
    uint8_t input[ST_BLOCKBYTES];
    uint8_t output[ST_BLOCKBYTES];
    st_crypto_f crypto;
    void* cryptoData;

    // Prcoess data
    int limit;
    uint8_t* split;
    int map[L1_SETS];
    uint8_t clusterMask;
    st_clusters_t clusters;
};

static void dummy_setup_cb(int nrecords, void* data) { return; }

// Synchronized Prime+Probe
int st_l1pp(l1pp_t l1, int nrecords, st_setup_cb setup, st_exec_cb exec,
            st_process_cb process, void* data) {
    assert(l1 != NULL);
    assert(exec != NULL);
    assert(nrecords >= 0);

    if (nrecords == 0)
        return 0;

    if (setup == NULL)
        setup = dummy_setup_cb;
    int len = l1_getmonitoredset(l1, NULL, 0);
    uint16_t* res = malloc(len * sizeof(uint16_t));

    for (int i = 0; i < nrecords; i++) {
        setup(i, data);
        l1_probe(l1, res);
        exec(i, data);
        l1_bprobe(l1, res);
        process(i, data, len, res);
    }

    free(res);
    return nrecords;
}

static void spp_setup(int recnum, void* vst) {
    p_probe_t st = (p_probe_t) vst;
    for (int i = 0; i < st->blockSize; i++)
        st->input[i] = (rand() & 0xff & ~st->fixMask[i])
                       | (st->fixData[i] & st->fixMask[i]);
}

static void spp_process(int recnum, void* vst, int nres, uint16_t results[]) {
    p_probe_t st = (p_probe_t) vst;
    for (int byte = 0; byte < st->blockSize; byte++) {
        int inputbyte = st->split[byte] & st->clusterMask;
        st->clusters[byte].count[inputbyte]++;
        for (int i = 0; i < nres; i++) {
            uint64_t res = results[i] > LIMIT ? LIMIT : results[i];

            st->clusters[byte].avg[inputbyte][st->map[i]] += res;
        }
    }
}

static void spp_exec(int recnum, void* vst) {
    p_probe_t st = (p_probe_t) vst;
    (*st->crypto)(st->input, st->output, st->cryptoData);
}

static void normalise(int blockSize, st_clusters_t clusters) {
    int nsets;
    nsets = L1_SETS;
    for (int byte = 0; byte < blockSize; byte++) {
        int total_count = 0;
        int64_t total_avg[nsets];
        memset(total_avg, 0, sizeof(total_avg));
        for (int cluster = 0; cluster < 256; cluster++) {
            if (clusters[byte].count[cluster]) {
                int count = clusters[byte].count[cluster];
                total_count += count;
                for (int set = 0; set < nsets; set++) {
                    int64_t avg = clusters[byte].avg[cluster][set];
                    total_avg[set] += avg;
                    avg = (avg * SCALE + count / 2) / count;
                    clusters[byte].avg[cluster][set] = avg;
                }
            }
        }
        for (int set = 0; set < nsets; set++)
            total_avg[set]
                = (total_avg[set] * SCALE + total_count / 2) / total_count;

        for (int cluster = 0; cluster < 256; cluster++)
            if (clusters[byte].count[cluster])
                for (int set = 0; set < nsets; set++)
                    clusters[byte].avg[cluster][set] -= total_avg[set];
    }
}

st_clusters_t syncPrimeProbe(int nsamples, int blockSize, int splitinput,
                             uint8_t* fixMask, uint8_t* fixData,
                             st_crypto_f crypto, void* cryptoData,
                             uint8_t clusterMask) {
    p_probe_t st = (p_probe_t) malloc(sizeof(struct p_probe));
    memset(st, 0, sizeof(struct p_probe));
    st->blockSize = blockSize;
    if (fixMask && fixData) {
        memcpy(st->fixMask, fixMask, blockSize);
        memcpy(st->fixData, fixData, blockSize);
    }
    st->crypto = crypto;
    st->cryptoData = cryptoData;
    st_clusters_t clusters = calloc(blockSize, sizeof(struct st_clusters));
    st->clusters = clusters;
    st->clusterMask = clusterMask;
    st->split = splitinput ? st->input : st->output;

    l1pp_t l1;
    l1 = l1_prepare(NULL);
    l1_monitorall(l1);
    l1_getmonitoredset(l1, st->map, l1->nsets);

    st_l1pp(l1, nsamples, spp_setup, spp_exec, spp_process, st);

    free(st);
    normalise(blockSize, clusters);
    l1_release(l1);
    return clusters;
}