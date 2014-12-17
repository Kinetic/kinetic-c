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
    int buckets[];
};

/* Init a hash table with approx. 2 ** sz2 buckets. */
struct yacht *yacht_init(uint8_t sz2) {
    size_t size = 1 << sz2;
    struct yacht *y = malloc(sizeof(struct yacht) + size * sizeof(int));
    if (y) {
        y->size = size;
        y->mask = size - 1;
        LOG(" -- init with size %zd\n", size);
        for (size_t i = 0; i < size; i++) {
            y->buckets[i] = YACHT_NO_KEY;
        }
        return y;
    } else {
        return NULL;
    }
}

#define LARGE_PRIME (4294967291L /* (2 ** 32) - 5 */)

static int hash(int key) {
    return key * LARGE_PRIME;
}

/* Check if KEY is in the table. */
bool yacht_member(struct yacht *y, int key) {
    size_t b = hash(key) & y->mask;
    LOG(" -- checking membership for %d with bucket %d\n", key, b);

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
}

/* Add KEY to the table. */
bool yacht_add(struct yacht *y, int key) {
    size_t b = hash(key) & y->mask;
    LOG(" -- adding %d with bucket %d\n", key, b);

    for (size_t i = b; i < y->size; i++) {
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) {
            return true;        /* already present */
        } else if (bv == YACHT_NO_KEY || bv == YACHT_DELETED) {
            y->buckets[i] = key;
            return true;
        } else {
            /* Brent's variation -- bump out key if not in its main spot  */
            size_t oh = hash(bv) & y->mask;
            if (oh != i) {
                y->buckets[i] = key;
                key = oh;
            }
        }
    }

    for (size_t i = 0; i < b; i++) { /* wrap, if necessary */
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) {
            return true;        /* already present */
        } else if (bv == YACHT_NO_KEY || bv == YACHT_DELETED) {
            y->buckets[i] = key;
            return true;
        } else {
            /* Brent's variation -- bump out key if not in its main spot  */
            size_t oh = hash(bv) & y->mask;
            if (oh != i) {
                y->buckets[i] = key;
                key = oh;
            }
        }
    }

    return false;
}

/* Remove KEY from the table. */
bool yacht_remove(struct yacht *y, int key) {
    size_t b = hash(key) & y->mask;
    LOG(" -- removing %d with bucket %d\n", key, b);

    for (size_t i = b; i < y->size; i++) {
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) {
            if (y->buckets[(i + 1) & y->mask] == YACHT_NO_KEY) {
                y->buckets[i] = YACHT_NO_KEY;
            } else {
                y->buckets[i] = YACHT_DELETED;
            }
            return true;
        } else if (bv == YACHT_NO_KEY) {
            return false;       /* not present */
        }
    }

    for (size_t i = 0; i < b; i++) { /* wrap, if necessary */
        int bv = y->buckets[i];
        LOG(" -- b %d: %d\n", i, bv);
        if (bv == key) {
            if (y->buckets[(i + 1) & y->mask] == YACHT_NO_KEY) {
                y->buckets[i] = YACHT_NO_KEY;
            } else {
                y->buckets[i] = YACHT_DELETED;
            }
            return true;
        } else if (bv == YACHT_NO_KEY) {
            return false;       /* not present */
        }
    }

    return false;
}

/* Free the table. */
void yacht_free(struct yacht *y) {
    free(y);
}
