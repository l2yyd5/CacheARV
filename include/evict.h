#ifndef __EVICT_H__
#define __EVICT_H__ 1

#include <stdlib.h>

#include <l1.h>

#define CACHE_ALIGN __attribute__((aligned(L1_CACHELINE)))

void evict_cache_set(uint64_t addr);

void evict();

#endif // __EVICT_H__