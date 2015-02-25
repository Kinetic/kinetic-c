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
#ifndef SENDER_INTERNAL_H
#define SENDER_INTERNAL_H

#include "sender.h"

typedef enum {
    HW_OK,
    HW_DONE,
    HW_ERROR = -1,
} handle_write_res;

static handle_write_res handle_write(bus *b, boxed_msg *box);
static void handle_failure(struct bus *b, boxed_msg *box, bus_send_status_t status);
static void attempt_to_enqueue_sending_request_message_to_listener(bus *b,
    int fd, int64_t seq_id, int16_t timeout_sec);

static void attempt_to_enqueue_sending_request_message_to_listener(struct bus *b,
    int fd, int64_t seq_id, int16_t timeout_sec);

#endif
