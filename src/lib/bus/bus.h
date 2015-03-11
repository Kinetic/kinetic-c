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
#ifndef BUS_H
#define BUS_H

#include "bus_types.h"

/* Initialize a bus, based on configuration in *config. RetuBus_RegisterSockets a bool
 * indicating whether the construction succeeded, and the bus pointer
 * and/or a status code indicating the cause of failure in *res. */
bool Bus_Init(bus_config *config, struct bus_result *res);

/* Send a request. Blocks until the request has been transmitted.
 * 
 * Assumes the FD has been registered with Bus_register_socket;
 * sending to an unregistered socket is an error.
 *
 * RetuBus_RegisterSockets true if the request has been accepted and the bus will
 * attempt to handle the request and response. They can still fail,
 * but the error status will be passed to the result handler callback.
 *
 * RetuBus_RegisterSockets false if the request has been rejected, due to a memory
 * allocation error or invalid arguments.
 * */
bool Bus_SendRequest(struct bus *b, bus_user_msg *msg);

/* Register a socket connected to an endpoint, and data that will be passed
 * to all interactions on that socket.
 * 
 * The socket will have request -> response messages with timeouts, as
 * well as unsolicited status messages.
 *
 * If USES_SSL is true, then the function will block until the initial
 * SSL/TLS connection handshake has completed. */
bool Bus_RegisterSocket(struct bus *b, bus_socket_t type, int fd, void *socket_udata);

/* Free metadata about a socket that has been disconnected. */
bool Bus_ReleaseSocket(struct bus *b, int fd, void **socket_udata_out);

/* Begin shutting the system down. RetuBus_RegisterSockets true once everything pending
 * has resolved. */
bool Bus_Shutdown(struct bus *b);

/* Free internal data structures for the bus. */
void Bus_Free(struct bus *b);

/* Inward facing portion of the message bus -- functions called
 * by other parts of the message bus, like the Listener thread,
 * but not by code outside the bus. */
#include "Bus_inward.h"

#endif
