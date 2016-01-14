/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */
#ifndef BUS_H
#define BUS_H

#include "bus_types.h"

/** Initialize a bus, based on configuration in *config. Returns a bool
 * indicating whether the construction succeeded, and the bus pointer
 * and/or a status code indicating the cause of failure in *res. */
bool Bus_Init(bus_config *config, struct bus_result *res);

/** Send a request. Blocks until the request has been transmitted.
 *
 * Assumes the FD has been registered with Bus_register_socket;
 * sending to an unregistered socket is an error.
 *
 * Returns true if the request has been accepted and the bus will
 * attempt to handle the request and response. They can still fail,
 * but the error status will be passed to the result handler callback.
 *
 * Returns false if the request has been rejected, due to a memory
 * allocation error or invalid arguments.
 * */
bool Bus_SendRequest(struct bus *b, bus_user_msg *msg);

/** Register a socket connected to an endpoint, and data that will be passed
 * to all interactions on that socket.
 *
 * The socket will have request -> response messages with timeouts, as
 * well as unsolicited status messages.
 *
 * If USES_SSL is true, then the function will block until the initial
 * SSL/TLS connection handshake has completed. */
bool Bus_RegisterSocket(struct bus *b, bus_socket_t type, int fd, void *socket_udata);

/** Free metadata about a socket that has been disconnected. */
bool Bus_ReleaseSocket(struct bus *b, int fd, void **socket_udata_out);

/** Begin shutting the system down. Returns true once everything pending
 * has resolved. */
bool Bus_Shutdown(struct bus *b);

/** Free internal data structures for the bus. */
void Bus_Free(struct bus *b);

/** Inward facing portion of the message bus -- functions called
 * by other parts of the message bus, like the Listener thread,
 * but not by code outside the bus. */
#include "bus_inward.h"

#endif
