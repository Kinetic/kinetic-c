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
#ifndef SEND_H
#define SEND_H

#include "bus_types.h"
#include "bus_internal_types.h"

/* Do a blocking send.
 *
 * Returning true indicates that the message has been queued up for
 * delivery, but the request or response may still fail. Those errors
 * are handled by giving an error status code to the callback.
 * Returning false means that the send was rejected outright, and
 * the callback-based error handling will not be used. */
bool send_do_blocking_send(struct bus *b, boxed_msg *box);

void send_handle_failure(struct bus *b, boxed_msg *box, bus_send_status_t status);

#endif
