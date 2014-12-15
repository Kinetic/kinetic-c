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
#ifndef CASQ_H
#define CASQ_H

#include <stdbool.h>

/* Atomic queue. */
struct casq;

struct casq *casq_new(void);

bool casq_push(struct casq *q, void *data);
void *casq_pop(struct casq *q);
bool casq_empty(struct casq *q);

typedef void (casq_free_cb)(void *data, void *udata);

void casq_free(struct casq *q, casq_free_cb *cb, void *udata);

#endif
