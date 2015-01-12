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
#ifndef SENDER_H
#define SENDER_H

#include "bus_types.h"
#include "bus_internal_types.h"

/* Manager of active outgoing messages. */
struct sender;

struct sender *sender_init(struct bus *b, struct bus_config *cfg);

/* Sender commands.
 *
 * These all block until the message has been processed (a command
 * has been sent over an outgoing socket, a socket has been registered
 * or unregistered, or the sender has shut down) or delivery has
 * failed.
 *
 * Backpressure is handled internally. */
bool sender_register_socket(struct sender *s, int fd, SSL *ssl);
bool sender_remove_socket(struct sender *s, int fd);

/* Send an outgoing message.
 * 
 * This blocks until the message has either been sent over a outgoing
 * socket or delivery has failed, to provide counterpressure. */
bool sender_send_request(struct sender *s, boxed_msg *box);

bool sender_shutdown(struct sender *s);

void sender_free(struct sender *s);


#endif
