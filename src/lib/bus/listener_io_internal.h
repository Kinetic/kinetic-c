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
#ifndef LISTENER_IO_INTERNAL_H
#define LISTENER_IO_INTERNAL_H

static bool socket_read_plain(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static bool socket_read_ssl(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static bool sink_socket_read(struct bus *b,
    listener *l, connection_info *ci, ssize_t size);
static void print_SSL_error(struct bus *b,
    connection_info *ci, int lvl, const char *prefix);
static void set_error_for_socket(listener *l, int id,
    int fd, rx_error_t err);
static void process_unpacked_message(listener *l,
    connection_info *ci, bus_unpack_cb_res_t result);
static rx_info_t *find_info_by_sequence_id(listener *l,
    int fd, int64_t seq_id);

#endif
