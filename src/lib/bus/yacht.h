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
#ifndef YACHT_H
#define YACHT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define YACHT_NO_KEY ((int)-1)
#define YACHT_DELETED ((int)-2)

/* Yet Another C Hash Table - (file descriptor int set) hash table.
 * The key cannot be YACHT_NO_KEY or YACHT_DELETED,
 * which is acceptable for file descriptors. */
struct yacht;

/* Init a hash table with approx. 2 ** sz2 buckets. */
struct yacht *yacht_init(uint8_t sz2);

/* Add KEY to the table. */
bool yacht_add(struct yacht *y, int key);

/* Check if KEY is in the table. */
bool yacht_member(struct yacht *y, int key);

/* Remove KEY from the table. */
bool yacht_remove(struct yacht *y, int key);

/* Free the table. */
void yacht_free(struct yacht *y);

#endif
