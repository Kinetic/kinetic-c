/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#ifndef YACHT_H
#define YACHT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/** Special value for no key in a hash bucket. */
#define YACHT_NO_KEY ((int)-1)

/** Special placeholder for a deleted key in a hash bucket. */
#define YACHT_DELETED ((int)-2)

/** Yet Another C Hash Table - (file descriptor int set) hash table.
 * The key cannot be YACHT_NO_KEY or YACHT_DELETED,
 * which is acceptable for file descriptors. */
struct yacht;

/** Init a hash table with approx. 2 ** sz2 buckets. */
struct yacht *Yacht_Init(uint8_t sz2);

/** Set KEY to VALUE in the table. */
bool Yacht_Set(struct yacht *y, int key, void *value, void **old_value);

/** Get KEY from the table, setting *value if found. */
bool Yacht_Get(struct yacht *y, int key, void **value);

/** Check if KEY is in the table. */
bool Yacht_Member(struct yacht *y, int key);

/** Remove KEY from the table. RetuBus_RegisterSocket the old value in *old_value,
 * if non-NULL. */
bool Yacht_Remove(struct yacht *y, int key, void **old_value);

/** Callback to free values associated with keys. */
typedef void (Yacht_Free_cb)(void *value, void *udata);

/** Free the table. */
void Yacht_Free(struct yacht *y, Yacht_Free_cb *cb, void *udata);

#ifdef TEST
#include "yacht_internals.h"
#endif

#endif
