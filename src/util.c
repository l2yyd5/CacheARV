#include <low.h>
#include <util.h>

void delayloop(uint32_t cycles) {
    uint32_t start = rdtscp();
    while ((rdtscp() - start) < cycles)
        ;
}
