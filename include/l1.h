/*
 * Copyright 2016 CSIRO
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

#ifndef __L1_H__
#define __L1_H__ 1

#define L1_SETS 64

#include <mm.h>
#include <stdio.h>

typedef void (*l1progressNotification_t)(int count, int est, void* data);

struct l1info {
    int associativity;
    int sets;
    int bufsize;
    int flags;
    void* progressNotificationData;

    // padding variables
    int slices;
    l1progressNotification_t progressNotification;
    union cpuid cpuidInfo;
};

#define LNEXT(t) (*(void**) (t))
#define OFFSET(p, o) ((void*) ((uint64_t) (p) + (o)))
#define NEXTPTR(p) (OFFSET((p), sizeof(void*)))

#define IS_MONITORED(monitored, setno)                                         \
    ((monitored)[(setno) >> 5] & (1 << ((setno) &0x1f)))
#define SET_MONITORED(monitored, setno)                                        \
    ((monitored)[(setno) >> 5] |= (1 << ((setno) &0x1f)))
#define UNSET_MONITORED(monitored, setno)                                      \
    ((monitored)[(setno) >> 5] &= ~(1 << ((setno) &0x1f)))

#define LX_CACHELINE 0x40

#define L1_ASSOCIATIVITY 8
#define L1_CACHELINE 64
#define L1_STRIDE (L1_CACHELINE * L1_SETS)

struct l1pp {
    void** monitoredhead;
    int nsets;

    int* monitored;

    uint32_t* monitoredbitmap;

    cachelevel_e level;
    size_t totalsets;

    int ngroups;
    int groupsize;
    vlist_t* groups;

    struct l1info l1info;

    mm_t mm;
    uint8_t internalmm;
};

typedef struct l1pp* l1pp_t;
typedef struct l1info* l1info_t;
typedef void (*l1_sync_cb)(l1pp_t l1, int recnum, void* data);

l1pp_t l1_prepare(mm_t mm);
void l1_release(l1pp_t l1);

int l1_monitor(l1pp_t l1, int line);
void l1_unmonitor(l1pp_t l1, int line);
void l1_monitorall(l1pp_t l1);
void l1_unmonitorall(l1pp_t l1);
int l1_getmonitoredset(l1pp_t l1, int* lines, int nlines);
void l1_randomise(l1pp_t l1);

void l1_probe(l1pp_t l1, uint16_t* results);
void l1_bprobe(l1pp_t l1, uint16_t* results);

// Synchronized Prime+Probe
void l1_syncpp(l1pp_t l1, int nrecords, uint16_t* results, l1_sync_cb setup,
               l1_sync_cb exec, void* data);
// Synchronized Evict+Time
int l1_syncet(l1pp_t l1, int nrecords, uint16_t* results, l1_sync_cb setup,
              l1_sync_cb exec, void* data);

void fillL1Info(l1info_t l1info);

#endif // __L1_H__
