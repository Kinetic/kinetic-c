#ifndef YACHT_INTERNALS_H
#define YACHT_INTERNALS_H

/* Yet Another C Hash Table:
 *   An (int -> void *metadata) hash table for tracking active file
 *   descriptors and their metadata. */
struct yacht {
    size_t size;
    size_t mask;
    int *buckets;
    void **values;
};

#endif
