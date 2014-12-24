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
#ifndef LISTENER_H
#define LISTENER_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "casq.h"

/* Manager of incoming messages from drives, both responses and
 * unsolicited status updates. */
struct listener;

struct listener *listener_init(struct bus *b, struct bus_config *cfg);

/* Add/remove sockets' metadata from internal info. */
bool listener_add_socket(struct listener *l, connection_info *ci, int notify_fd);
bool listener_remove_socket(struct listener *l, int fd);

bool listener_expect_response(struct listener *l, boxed_msg *box,
    uint16_t *backpressure);

bool listener_shutdown(struct listener *l);

void listener_free(struct listener *l);

#endif
