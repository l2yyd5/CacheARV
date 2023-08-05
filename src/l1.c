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

#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>

#include <l1.h>
#include <low.h>

static void return_linked_memory(l1pp_t l1, void* head) {
    void* p = head;
    do {
        mm_returnline(l1->mm, head);
        head = *((void**) head);
    } while (head != p);
}

int probetime(void* pp) {
    if (pp == NULL)
        return 0;
    int rv = 0;
    void* p = (void*) pp;
    uint32_t s = rdtscp();
    do {
        p = LNEXT(p);
    } while (p != (void*) pp);
    return rdtscp() - s;
}

int bprobetime(void* pp) {
    if (pp == NULL)
        return 0;
    return probetime(NEXTPTR(pp));
}

int probecount(void* pp) {
    if (pp == NULL)
        return 0;
    int rv = 0;
    void* p = (void*) pp;
    do {
        uint32_t s = rdtscp();
        p = LNEXT(p);
        s = rdtscp() - s;
        if (s > L3_THRESHOLD)
            rv++;
    } while (p != (void*) pp);
    return rv;
}

int bprobecount(void* pp) {
    if (pp == NULL)
        return 0;
    return probecount(NEXTPTR(pp));
}

void fillL1Info(l1info_t l1info) {
    if (l1info->associativity == 0)
        l1info->associativity = 8;
    if (l1info->sets == 0)
        l1info->sets = 64;
    if (l1info->bufsize == 0)
        l1info->bufsize = L1_STRIDE * l1info->associativity;
}

l1pp_t l1_prepare(mm_t mm) {
    l1pp_t l1 = (l1pp_t) malloc(sizeof(struct l1pp));
    memset(l1, 0, sizeof(struct l1pp));

    fillL1Info(&l1->l1info);

    l1->mm = mm;
    if (l1->mm == NULL) {
        l1->mm = mm_prepare(NULL, NULL, NULL);
        l1->internalmm = 1;
    }

    l1->monitored = (int*) malloc(L1_SETS * sizeof(int));
    l1->monitoredhead = (void**) malloc(L1_SETS * sizeof(void*));
    l1->monitoredbitmap
        = (uint32_t*) calloc((L1_SETS / 32) + 1, sizeof(uint32_t));
    l1->level = L1;
    l1->totalsets = L1_SETS;
    l1_monitorall(l1);
    return l1;
}

void l1_release(l1pp_t l1) {
    free(l1->monitoredbitmap);
    free(l1->monitored);
    free(l1->monitoredhead);
    if (l1->internalmm)
        mm_release(l1->mm);
    memset(l1, 0, sizeof(struct l1pp));
    free(l1);
}

int l1_monitor(l1pp_t l1, int line) {
    if (line < 0 || line >= l1->totalsets)
        return 0;
    if (IS_MONITORED(l1->monitoredbitmap, line))
        return 0;

    int associativity = 8;

    vlist_t vl = vl_new();
    _mm_requestlines(l1->mm, l1->level, line, associativity, vl);

    int len = vl_len(vl);
    for (int way = 0; way < len; way++) {
        void* mem = vl_get(vl, way);
        void* nmem = vl_get(vl, (way + 1) % len);
        void* pmem = vl_get(vl, (way - 1 + len) % len);

        LNEXT(mem) = nmem;
        LNEXT(mem + sizeof(void*)) = (pmem + sizeof(void*));
    }
    l1->monitored[l1->nsets] = line;
    l1->monitoredhead[l1->nsets++] = vl_get(vl, 0);
    SET_MONITORED(l1->monitoredbitmap, line);
    vl_free(vl);
    return 1;
}

void l1_unmonitor(l1pp_t l1, int line) {
    if (line < 0 || line >= l1->totalsets)
        return;
    if (!IS_MONITORED(l1->monitoredbitmap, line))
        return;
    UNSET_MONITORED(l1->monitoredbitmap, line);
    for (int i = 0; i < l1->nsets; i++)
        if (l1->monitored[i] == line) {
            --l1->nsets;
            l1->monitored[i] = l1->monitored[l1->nsets];

            return_linked_memory(l1, l1->monitoredhead[i]);

            l1->monitoredhead[i] = l1->monitoredhead[l1->nsets];

            break;
        }
    return;
}

void l1_monitorall(l1pp_t l1) {
    for (int i = 0; i < l1->totalsets; i++)
        l1_monitor(l1, i);
}

void l1_unmonitorall(l1pp_t l1) {
    for (int i = 0; i < l1->totalsets / 32; i++)
        l1->monitoredbitmap[i] = 0;
    for (int i = 0; i < l1->nsets; i++)
        return_linked_memory(l1, l1->monitoredhead[i]);
    l1->nsets = 0;
}

int l1_getmonitoredset(l1pp_t l1, int* lines, int nlines) {
    if (lines == NULL || nlines == 0)
        return l1->nsets;
    if (nlines > l1->nsets)
        nlines = l1->nsets;
    if (lines)
        memcpy(lines, l1->monitored, nlines * sizeof(int));
    return l1->nsets;
}

int l1_nsets(l1pp_t l1) { return l1->nsets; }

void l1_randomise(l1pp_t l1) {
    for (int i = 0; i < l1->nsets; i++) {
        int p = rand() % (l1->nsets - i) + i;
        int t = l1->monitored[p];
        l1->monitored[p] = l1->monitored[i];
        l1->monitored[i] = t;

        void* vt = l1->monitoredhead[p];
        l1->monitoredhead[p] = l1->monitoredhead[i];
        l1->monitoredhead[i] = vt;
    }
}

void l1_probe(l1pp_t l1, uint16_t* results) {
    for (int i = 0; i < l1->nsets; i++) {
        int t = probetime(l1->monitoredhead[i]);
        results[i] = t > UINT16_MAX ? UINT16_MAX : t;
    }
}

void l1_bprobe(l1pp_t l1, uint16_t* results) {
    for (int i = 0; i < l1->nsets; i++) {
        int t = bprobetime(l1->monitoredhead[i]);
        results[i] = t > UINT16_MAX ? UINT16_MAX : t;
    }
}

static void l1_dummy_cb(l1pp_t l1, int recnum, void* data) { return; }

// Synchronized Prime+Probe
void l1_syncpp(l1pp_t l1, int nrecords, uint16_t* results, l1_sync_cb setup,
               l1_sync_cb exec, void* data) {
    assert(l1 != NULL);
    assert(results != NULL);
    assert(exec != NULL);
    assert(nrecords >= 0);

    if (nrecords == 0)
        return;

    if (setup == NULL)
        setup = l1_dummy_cb;
    int len = l1->nsets;

    for (int i = 0; i < nrecords; i++, results += len) {
        setup(l1, i, data);

        l1_probe(l1, results);

        exec(l1, i, data);

        l1_bprobe(l1, results);
    }
}

// Synchronized Evict+Time
int l1_syncet(l1pp_t l1, int nrecords, uint16_t* results, l1_sync_cb setup,
              l1_sync_cb exec, void* data) {
    assert(l1 != NULL);
    assert(results != NULL);
    assert(exec != NULL);
    assert(nrecords >= 0);

    uint16_t dummyres[64];

    if (nrecords == 0)
        return 0;

    if (setup == NULL)
        setup = l1_dummy_cb;
    int len = l1->nsets;

    for (int i = 0; i < nrecords; i++, results++) {
        setup(l1, i, data);
        exec(l1, i, data);
        l1_probe(l1, dummyres);
        l1_probe(l1, dummyres);
        l1_probe(l1, dummyres);
        uint32_t start = rdtscp();
        exec(l1, i, data);
        uint32_t res = rdtscp() - start;
        *results = res > UINT16_MAX ? UINT16_MAX : res;
    }
    return nrecords;
}
