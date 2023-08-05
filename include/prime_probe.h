#ifndef __PRIME_PROBE_H__
#define __PRIME_PROBE_H__ 1

#include <l1.h>

#include <stdint.h>

// Type of callback for setup
typedef void (*st_setup_cb)(int recnum, void* data);
// Type of callback for exec
typedef void (*st_exec_cb)(int recnum, void* data);
// Type of callback for results processing
typedef void (*st_process_cb)(int recnum, void* data, int nres,
                              uint16_t results[]);

// Synchronized Prime+Probe
int st_l1pp(l1pp_t l1, int nrecords, st_setup_cb setup, st_exec_cb exec,
            st_process_cb process, void* data);

#define ST_BLOCKBITS 512
#define ST_BLOCKBYTES (ST_BLOCKBITS >> 3)
#define ST_TRACEWIDTH L1_SETS

typedef void (*st_crypto_f)(uint8_t input[], uint8_t output[],
                            void* cryptoData);

struct st_clusters {
    int count[256];
    int64_t avg[256][L1_SETS];
};

typedef struct st_clusters* st_clusters_t;

st_clusters_t syncPrimeProbe(int nsamples, int blocksize, int splitinput,
                             uint8_t* fixMask, uint8_t* fixData,
                             st_crypto_f crypto, void* cryptoData,
                             uint8_t clusterMask);

#endif // __PRIME_PROBE_H__
