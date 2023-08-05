#ifndef __LOW_H__
#define __LOW_H__ 1

#include <stdint.h>

#define L3_THRESHOLD 140

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static inline int memaccess(void* p) {
    int rv;
    asm volatile("lw %0,0(%1)" : "+r"(rv) : "r"(p) :);
    return rv;
}

static inline uint32_t memaccesstime(void* v) {
    uint32_t rv;
    asm volatile("fence rw, rw\n\t"
                 "rdcycle s2\n\t"
                 "mv s3, s2\n\t"
                 "lb s2, 0(%1)\n\t"
                 "rdcycle s2\n\t"
                 "sub %0, s2, s3\n\t"
                 : "=r"(rv)
                 : "r"(v)
                 : "s2", "s3");
    return rv;
}

static inline void cflush(void* v) {
    asm volatile("addi sp, sp, -8\n\t"
                 "sd t3, 0(sp)\n\t"
                 "mv t3, %0\n\t"
                 ".word 0xfff03077\n\t"
                 "ld t3, 0(sp)\n\t"
                 "addi sp, sp, 8\n\t"
                 :
                 : "r"(v)
                 : "t3");
}

static inline uint32_t rdtscp() {
    uint32_t rv;
    asm volatile("rdcycle %0" : "=r"(rv) :);
    return rv;
}

static inline void lfence() { asm volatile("fence r,r\n\t" ::); }

static inline void sfence() { asm volatile("fence w,w\n\t" ::); }

static inline void mfence() { asm volatile("fence rw,rw\n\t" ::); }

#define CPUID_CACHEINFO 4

#define CACHETYPE_NULL 0
#define CACHETYPE_DATA 1
#define CACHETYPE_INSTRUCTION 2
#define CACHETYPE_UNIFIED 3

struct cpuidCacheInfo {
    uint32_t type : 5;
    uint32_t level : 3;
    uint32_t selfInitializing : 1;
    uint32_t fullyAssociative : 1;
    uint32_t reserved1 : 4;
    uint32_t logIds : 12;
    uint32_t phyIds : 6;

    uint32_t lineSize : 12;
    uint32_t partitions : 10;
    uint32_t associativity : 10;

    uint32_t sets : 32;

    uint32_t wbinvd : 1;
    uint32_t inclusive : 1;
    uint32_t complexIndex : 1;
    uint32_t reserved2 : 29;
};

union cpuid {
    struct cpuidCacheInfo cacheInfo;
};

static inline int slotwait(uint32_t slotend) {
    if (rdtscp() > slotend)
        return 1;
    while (rdtscp() < slotend)
        ;
    return 0;
}

#endif //__LOW_H__
