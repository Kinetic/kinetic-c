/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#ifndef YACHT_INTERNALS_H
#define YACHT_INTERNALS_H

/* Yet Another C Hash Table:
 *   An (int -> void *metadata) hash table for tracking active file
 *   descriptors and their metadata. */
struct yacht {
    size_t size;        ///< Size of bucket array.
    size_t mask;        ///< Mask for hash table bucket array.
    int *buckets;       ///< Hash table buckets (keys).
    void **values;      ///< Values associated with keys, if any.
};

#endif
