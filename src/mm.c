/*
 * Copyright 2021 The University of Adelaide
 *
 * This file is part of Mastik.
 *
 * Mastik is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mastik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mastik.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>

#include <l1.h>
#include <low.h>
#include <mm.h>
#include <vlist.h>

static uintptr_t getphysaddr(void* p);

static void* allocate_buffer(mm_t mm) {
    // Allocate cache buffer
    int bufsize;
    char* buffer = MAP_FAILED;
    bufsize = mm->l3info.bufsize;
    if (buffer == MAP_FAILED) {
        mm->pagetype = PAGETYPE_SMALL;
        mm->l3groupsize = 64;
        mm->pagesize = 4096;
        buffer = mmap(NULL, bufsize, PROT_READ | PROT_WRITE, 0x20 | MAP_PRIVATE,
                      -1, 0);
    }
    if (buffer == MAP_FAILED) {
        perror("Error allocating buffer!\n");
        exit(-1);
    }

    memset(buffer, 0, bufsize);
    return buffer;
}

mm_t mm_prepare(lxinfo_t l1info, lxinfo_t l2info, lxinfo_t l3info) {
    mm_t mm = calloc(1, sizeof(struct mm));

    if (l1info)
        memcpy(&mm->l1info, l1info, sizeof(struct lxinfo));

    fillL1Info((l1info_t) &mm->l1info);
    mm->l3info.bufsize = 10 * 1024 * 1024;

    mm->memory = vl_new();
    vl_push(mm->memory, allocate_buffer(mm));

    return mm;
}

void mm_release(mm_t mm) {
    int len = vl_len(mm->memory);
    size_t bufferSize = mm->l3info.bufsize;
    for (int i = 0; i < len; i++) {
        munmap(vl_get(mm->memory, i), bufferSize);
    }
    if (mm->l3buffer) {
        munmap(mm->l3buffer, bufferSize);
    }
    if (mm->l3groups) {
        for (int i = 0; i < mm->l3ngroups; i++)
            vl_free(mm->l3groups[i]);
        free(mm->l3groups);
    }
    vl_free(mm->memory);
    free(mm);
}

#define GET_GROUP_ID(buf)                                                      \
    (*((uint64_t*) ((uintptr_t) buf + 2 * sizeof(uint64_t))))
#define SET_GROUP_ID(buf, id)                                                  \
    (*((uint64_t*) ((uintptr_t) buf + 2 * sizeof(uint64_t))) = id)

#define CHECK_ALLOCATED_FLAG(buf, offset)                                      \
    (*((uint64_t*) ((uintptr_t) buf + offset + 3 * sizeof(uint64_t))))
#define SET_ALLOCATED_FLAG(buf, offset)                                        \
    (*((uint64_t*) ((uintptr_t) buf + offset + 3 * sizeof(uint64_t))) = 1ul)
#define UNSET_ALLOCATED_FLAG(buf)                                              \
    (*((uint64_t*) ((uintptr_t) buf + 3 * sizeof(uint64_t))) = 0ul)

#define L2_STRIDE ((mm->l2info.sets * L2_CACHELINE))

static void mm_l1l2findlines(mm_t mm, cachelevel_e cachelevel, int line,
                             int count, vlist_t list) {
    assert(list != NULL);

    uintptr_t stride;
    if (cachelevel == L1) {
        stride = L1_STRIDE;
    }

    int i = 0;
    while (count > 0) {
        int list_len = vl_len(mm->memory);
        for (; i < list_len; i++) {
            void* buf = vl_get(mm->memory, i);
            for (uintptr_t offset = line * LX_CACHELINE;
                 offset < mm->l3info.bufsize; offset += stride) {
                if (!CHECK_ALLOCATED_FLAG(buf, offset)) {
                    SET_ALLOCATED_FLAG(buf, offset);
                    vl_push(list, buf + offset);
                    if (--count == 0)
                        return;
                }
            }
        }
        vl_push(mm->memory, allocate_buffer(mm));
    }
    return;
}

static int parity(uint64_t v) {
    v ^= v >> 1;
    v ^= v >> 2;
    v = (v & 0x1111111111111111UL) * 0x1111111111111111UL;
    return (v >> 60) & 1;
}

#define SLICE_MASK_0 0x1b5f575440UL
#define SLICE_MASK_1 0x2eb5faa880UL
#define SLICE_MASK_2 0x3cccc93100UL

static int addr2slice_linear(uintptr_t addr, int slices) {
    int bit0 = parity(addr & SLICE_MASK_0);
    int bit1 = parity(addr & SLICE_MASK_1);
    int bit2 = parity(addr & SLICE_MASK_2);
    return ((bit2 << 2) | (bit1 << 1) | bit0) & (slices - 1);
}

static uintptr_t getphysaddr(void* p) {
#ifdef __linux__
    static int fd = -1;

    if (fd < 0) {
        fd = open("/proc/self/pagemap", O_RDONLY);
        if (fd < 0)
            return 0;
    }
    uint64_t buf;
    memaccess(p);
    uintptr_t intp = (uintptr_t) p;
    int r = pread(fd, &buf, sizeof(buf), ((uintptr_t) p) / 4096 * sizeof(buf));

    return (buf & ((1ULL << 54) - 1)) << 12 | ((uintptr_t) p & 0x3ff);
#else
    return 0;
#endif
}

#define CHECKTIMES 16

void _mm_requestlines(mm_t mm, cachelevel_e cachelevel, int line, int count,
                      vlist_t list) {
    switch (cachelevel) {
        case L1:
            mm_l1l2findlines(mm, cachelevel, line, count, list);
            break;
        default:
            break;
    }
}

void* mm_requestline(mm_t mm, cachelevel_e cachelevel, int line) {
    vlist_t vl = vl_new();
    _mm_requestlines(mm, cachelevel, line, 1, vl);
    void* mem = vl_get(vl, 0);
    vl_free(vl);
    return mem;
}

void mm_requestlines(mm_t mm, cachelevel_e cachelevel, int line, void** lines,
                     int count) {
    vlist_t vl = vl_new();
    _mm_requestlines(mm, cachelevel, line, count, vl);
    for (int i = 0; i < count; i++) {
        lines[i] = vl_get(vl, i);
    }
    vl_free(vl);
    return;
}

void mm_returnline(mm_t mm, void* line) { UNSET_ALLOCATED_FLAG(line); }

void _mm_returnlines(mm_t mm, vlist_t lines) {
    int len = vl_len(lines);
    for (int i = 0; i < len; i++)
        mm_returnline(mm, vl_get(lines, i));
}

void mm_returnlines(mm_t mm, void** lines, int count) {
    for (int i = 0; i < count; i++) {
        mm_returnline(mm, lines[i]);
    }
}