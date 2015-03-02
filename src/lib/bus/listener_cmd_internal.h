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
#ifndef LISTENER_CMD_INTERNAL_H
#define LISTENER_CMD_INTERNAL_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener_cmd.h"

static void msg_handler(listener *l, listener_msg *pmsg);
static void add_socket(listener *l, connection_info *ci, int notify_fd);
static void remove_socket(listener *l, int fd, int notify_fd);
static void hold_response(listener *l, int fd, int64_t seq_id, int16_t timeout_sec);
static void expect_response(listener *l, boxed_msg *box);
static void shutdown(listener *l, int notify_fd);

static rx_info_t *get_free_rx_info(struct listener *l);
static rx_info_t *get_hold_rx_info(listener *l, int fd, int64_t seq_id);

#endif
