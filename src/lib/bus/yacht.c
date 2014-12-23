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
 *   An (int set) hash table for tracking active file descriptors. */
struct yacht {
    size_t size;
    size_t mask;
    int *buckets;
    void **values;
};

/* Init a hash table with approx. 2 ** sz2 buckets. */
struct yacht *yacht_init(uint8_t sz2) {
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

static int hash(int key) {
    return key * LARGE_PRIME;
}

bool yacht_get(struct yacht *y, int key, void **value) {
    size_t b = hash(key) & y->mask;
    LOG(" -- getting %d with bucket %d\n", key, b);

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
#if 0    
    size_t b = hash(key) & y->mask;

    for (size_t i = b; i < y->size; i++) {
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) { return true; }
        if (bv == YACHT_NO_KEY) { return false; }
    }

    for (size_t i = 0; i < b; i++) { /* wrap, if necessary */
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) { return true; }
        if (bv == YACHT_NO_KEY) { return false; }
    }

    return false;
#endif
}

/* Set KEY to VALUE in the table. */
bool yacht_set(struct yacht *y, int key, void *value, void **old_value) {
    size_t b = hash(key) & y->mask;
    LOG(" -- adding %d with bucket %d\n", key, b);

    for (size_t o = 0; o < y->size; o++) {
        size_t i = (b + o) & y->mask;  // wrap as necessary
        //for (size_t i = b; i < y->size; i++) {
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) {
            if (old_value) { *old_value = y->values[i]; }
            y->values[i] = value;
            return true;        /* already present */
        } else if (bv == YACHT_NO_KEY || bv == YACHT_DELETED) {
            y->buckets[i] = key;
            y->values[i] = value;
            return true;
        } else {
            /* Brent's variation -- bump out key if not in its main spot  */
            size_t oh = hash(bv) & y->mask;
            if (oh != i) {
                int okey = y->buckets[i];
                y->buckets[i] = key;
                key = okey;
                void *oval = y->values[i];
                y->values[i] = value;
                value = oval;
            }
        }
    }

    return false;
}

/* Remove KEY from the table. */
bool yacht_remove(struct yacht *y, int key, void **old_value) {
    size_t b = hash(key) & y->mask;
    LOG(" -- removing %d with bucket %d\n", key, b);

    for (size_t o = 0; o < y->size; o++) {
        //for (size_t i = b; i < y->size; i++) {
        size_t i = (b + o) & y->mask;  // wrap as necessary
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
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
        free(y);
    }
}
