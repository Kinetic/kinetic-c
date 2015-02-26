/*
* kinetic-c-client
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "yacht.h"

#if 0
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

/* Yet Another C Hash Table:
 *   An (int -> void *metadata) hash table for tracking active file
 *   descriptors and their metadata. */
struct yacht {
    size_t size;
    size_t mask;
    int *buckets;
    void **values;
};

static bool insert(int *buckets, void **values,
    size_t mask, size_t max_fill,
    int key, void *value, void **old_value);
static bool grow(struct yacht *y);

#define DEF_SZ2 4
#define MAX_PROBES 16

/* Init a hash table with approx. 2 ** sz2 buckets. */
struct yacht *yacht_init(uint8_t sz2) {
    if (sz2 == 0) { sz2 = DEF_SZ2; }
    size_t size = 1 << sz2;
    struct yacht *y = calloc(1, sizeof(*y));
    int *buckets = calloc(size, sizeof(*buckets));
    void **values = calloc(size, sizeof(*values));
    if (y && buckets && values) {
        y->size = size;
        y->mask = size - 1;
        y->buckets = buckets;
        y->values = values;
        LOG(" -- init with size %zd\n", size);
        for (size_t i = 0; i < size; i++) {
            y->buckets[i] = YACHT_NO_KEY;
        }
        return y;
    } else {
        if (y) { free(y); }
        if (buckets) { free(buckets); }
        if (values) { free(values); }
        return NULL;
    }
}

#define LARGE_PRIME (4294967291L /* (2 ** 32) - 5 */)

static size_t hash(int key) {
    return key * LARGE_PRIME;
}

bool yacht_get(struct yacht *y, int key, void **value) {
    size_t b = hash(key) & y->mask;
    LOG(" -- getting key %d with bucket %zd\n", key, b);

    for (size_t o = 0; o < y->size; o++) {
        size_t i = (b + o) & y->mask;  // wrap as necessary
        int bv = y->buckets[i];
        if (bv == key) {
            if (value) { *value = y->values[i]; }
            return true;
        } else if (bv == YACHT_NO_KEY || bv == YACHT_DELETED) {
            return false;
        }
    }
    return false;
}

/* Check if KEY is in the table. */
bool yacht_member(struct yacht *y, int key) {
    LOG(" -- checking membership for %d\n", key);
    return yacht_get(y, key, NULL);
}

/* Set KEY to VALUE in the table. */
bool yacht_set(struct yacht *y, int key, void *value, void **old_value) {
    LOG(" -- setting key %d\n", key);

    for (;;) {
        size_t max = y->size / 2;
        if (max > 16) { max = MAX_PROBES; }
        if (insert(y->buckets, y->values, y->mask, max, key, value, old_value)) {
            return true;
        } else {
            if (!grow(y)) { return false; }
        }
    }
}

static bool insert(int *buckets, void **values,
        size_t mask, size_t max_fill,
        int key, void *value, void **old_value) {
    size_t b = hash(key) & mask;

    for (size_t o = 0; o < max_fill; o++) {
        if (o > 0) { LOG(" -- o %zd (max_fill %zd)\n", o, max_fill); }
        size_t i = (b + o) & mask;  // wrap as necessary
        int bv = buckets[i];
        LOG(" -- b %zd: %d\n", i, bv);
        if (bv == key) {
            if (old_value) { *old_value = values[i]; }
            values[i] = value;
            return true;        /* already present */
        } else if (bv == YACHT_NO_KEY || bv == YACHT_DELETED) {
            buckets[i] = key;
            values[i] = value;
            return true;
        }
    }
    return false;               /* too full */
}

static bool grow(struct yacht *y) {
    size_t nsize = 2 * y->size;
    LOG(" -- growing %zd => %zd\n", y->size, nsize);
    size_t nmask = nsize - 1;
    int *nbuckets = NULL;
    void **nvalues = NULL;
    nbuckets = malloc(nsize * sizeof(*nbuckets));
    if (nbuckets == NULL) { return false; }
    for (size_t i = 0; i < nsize; i++) {
        nbuckets[i] = YACHT_NO_KEY;
    }
    nvalues = calloc(nsize, sizeof(*nvalues));
    if (nvalues == NULL) {
        free(nbuckets);
        return false;
    }

    for (size_t i = 0; i < y->size; i++) {
        int key = y->buckets[i];
        if (key != YACHT_NO_KEY && key != YACHT_DELETED) {
            if (!insert(nbuckets, nvalues, nmask, nsize, key, y->values[i], NULL)) {
                goto cleanup;
            }
        }
    }
    y->size = nsize;
    free(y->buckets);
    free(y->values);
    y->buckets = nbuckets;
    y->values = nvalues;
    y->mask = nmask;
    return true;
cleanup:
    if (nbuckets) { free(nbuckets); }
    if (nvalues) { free(nvalues); }

    return false;
}

/* Remove KEY from the table. */
bool yacht_remove(struct yacht *y, int key, void **old_value) {
    size_t b = hash(key) & y->mask;
    LOG(" -- removing %d with bucket %zd\n", key, b);

    for (size_t o = 0; o < y->size; o++) {
        size_t i = (b + o) & y->mask;  // wrap as necessary
        int bv = y->buckets[i];
        LOG(" -- b %zd: %d\n", i, bv);
        if (bv == key) {
            if (y->buckets[(i + 1) & y->mask] == YACHT_NO_KEY) {
                y->buckets[i] = YACHT_NO_KEY;
            } else {
                y->buckets[i] = YACHT_DELETED;
            }
            if (old_value) {
                *old_value = y->values[i];
                y->values[i] = NULL;
            }
            return true;
        } else if (bv == YACHT_NO_KEY) {
            return false;       /* not present */
        }
    }

    return false;
}

/* Free the table. */
void yacht_free(struct yacht *y, yacht_free_cb *cb, void *udata) {
    if (y) {
        for (size_t i = 0; i < y->size; i++) {
            int key = y->buckets[i];
            if (cb && key != YACHT_NO_KEY && key != YACHT_DELETED) {
                cb(y->values[i], udata);
            }
        }
        free(y->buckets);
        free(y->values);
        free(y);
    }
}
