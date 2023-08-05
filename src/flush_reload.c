#include <string.h>

#include <flush_reload.h>

size_t array[1024][64];

int get_threshold() {
    size_t hit_histogram[80];
    size_t miss_histogram[80];
    int sum1 = 0, sum2 = 0;
    memset(hit_histogram, 0, 80 * sizeof(size_t));
    memset(miss_histogram, 0, 80 * sizeof(size_t));

    for (int i = 0; i < 1024 * 1024; ++i) {
        size_t d = memaccesstime(array);
        sum1 += d;
        hit_histogram[MIN(79, d)]++;
    }

    for (int i = 0; i < 1024 * 1024; ++i) {
        cflush(NULL);
        size_t d = memaccesstime(array);
        sum2 += d;
        miss_histogram[MIN(79, d)]++;
    }

    size_t hit_max = 0;
    size_t hit_max_i = 0;
    size_t miss_min_i = 0;
    for (int i = 0; i < 80; ++i) {
        if (hit_max < hit_histogram[i]) {
            hit_max = hit_histogram[i];
            hit_max_i = i;
        }
        if (miss_histogram[i] > 20 && miss_min_i == 0)
            miss_min_i = i;
    }
    size_t min = -1UL;
    size_t min_i = 0;
    for (int i = hit_max_i; i < miss_min_i; ++i) {
        if (min > (hit_histogram[i] + miss_histogram[i])) {
            min = hit_histogram[i] + miss_histogram[i];
            min_i = i;
        }
    }

    printf("Suggested cache hit/miss threshold: %zu\n", min_i);
    return min_i;
}
