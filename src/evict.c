#include <evict.h>

uint8_t
    cache_set_array[L1_CACHELINE * L1_SETS * L1_ASSOCIATIVITY * 2] CACHE_ALIGN;

void evict_cache_set(uint64_t addr) {
    uint64_t set_index = (addr >> 6) & 0x3f;
    uint64_t start_set = ((uint64_t) cache_set_array >> 6) & 0x3f;
    int set_offset = (set_index + L1_SETS - start_set) % L1_SETS;
    mfence();
    for (int i = 0; i < L1_ASSOCIATIVITY * 2; i++) {
        uint8_t* target
            = &cache_set_array[((set_offset + i * L1_SETS)) * L1_CACHELINE];
        *target = rand() & 0xff;
    }
    mfence();
}

void evict() {
    mfence();
    for (int i = 0; i < L1_SETS * L1_ASSOCIATIVITY; i++) {
        uint8_t* target = &cache_set_array[i * L1_CACHELINE];
        *target = (rand() & 0xff);
    }
    mfence();
}