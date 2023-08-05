#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aes.h>
#include <evict_time.h>
#include <l1.h>
#include <low.h>

#define LIMIT 400
#define SCALE 1000

typedef struct e_time* e_time_t;

struct e_time {
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

// Synchronized Evict+Time
int st_l1et(l1pp_t l1, int nrecords, st_setup_cb setup, st_exec_cb exec,
            st_process_cb process, void* data) {
    assert(l1 != NULL);
    assert(exec != NULL);
    assert(nrecords >= 0);

    e_time_t st = (e_time_t) data;
    if (setup == NULL)
        setup = dummy_setup_cb;

    int len = l1_getmonitoredset(l1, NULL, 0);
    uint16_t* results = malloc(len * sizeof(uint16_t));

    for (int i = 0; i < nrecords; i++) {
        memset(results, 0, len * sizeof(uint16_t));
        for (int j = 0; j < len; j++) {
            setup(i, data);
            exec(i, data);
            exec(i, data);
            exec(i, data);
            exec(i, data);
            mfence();
            cflush(AES_get_ttable(1));
            mfence();
            uint32_t start = rdtscp();
            exec(i, data);
            uint32_t use_time = rdtscp() - start;
            results[j] = use_time > UINT16_MAX ? UINT16_MAX : use_time;
        }
    }
    return nrecords;
}

static void set_setup(int recnum, void* vst) {
    e_time_t st = (e_time_t) vst;
    for (int i = 0; i < st->blockSize; i++)
        st->input[i] = (rand() & 0xff & ~st->fixMask[i])
                       | (st->fixData[i] & st->fixMask[i]);
}

static void set_exec(int recnum, void* vst) {
    e_time_t st = (e_time_t) vst;
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

st_clusters_t syncEvictTime(int nsamples, int blockSize, uint8_t* fixMask,
                            uint8_t* fixData, st_crypto_f crypto,
                            void* cryptoData, uint8_t clusterMask) {
    e_time_t st = (e_time_t) malloc(sizeof(struct e_time));
    memset(st, 0, sizeof(struct e_time));
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
    st->split = st->input;

    l1pp_t l1 = l1_prepare(NULL);
    l1_monitorall(l1);
    l1_getmonitoredset(l1, st->map, L1_SETS);
    st_l1et(l1, nsamples, set_setup, set_exec, NULL, st);

    free(st);
    normalise(blockSize, clusters);
    l1_release(l1);
    return clusters;
}
