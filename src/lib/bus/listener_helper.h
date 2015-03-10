/*
* kinetic-c-client
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
#ifndef LISTENER_HELPER_H
#define LISTENER_HELPER_H

#include "bus_internal_types.h"
#include "listener.h"
#include "listener_internal_types.h"

listener_msg *listener_helper_get_free_msg(listener *l);
bool listener_helper_push_message(struct listener *l, listener_msg *msg, int *reply_fd);

rx_info_t *listener_helper_get_free_rx_info(listener *l);
rx_info_t *listener_helper_find_info_by_sequence_id(listener *l,
    int fd, int64_t seq_id);

#endif
