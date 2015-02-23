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
#ifndef BUS_SSL_H
#define BUS_SSL_H

#include "bus.h"
#include "bus_internal_types.h"

/* Default to TLS 1.1 for now, since it's what the drives support. */
#ifndef KINETIC_USE_TLS_1_2
#define KINETIC_USE_TLS_1_2 0
#endif

/* Initialize the SSL library internals for use by the messaging bus. */
bool bus_ssl_init(struct bus *b);

/* Do an SSL / TLS shake for a connection. Blocking.
 * Returns whether the connection succeeded. */
bool bus_ssl_connect(struct bus *b, connection_info *ci);

/* Disconnect and free an individual SSL handle. */
bool bus_ssl_disconnect(struct bus *b, SSL *ssl);

/* Free all internal data for using SSL (the SSL_CTX). */
void bus_ssl_ctx_free(struct bus *b);

#endif
