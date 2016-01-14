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

#ifndef BUS_SSL_H
#define BUS_SSL_H

#include "bus.h"
#include "bus_internal_types.h"

/* Default to TLS 1.1 for now, since it's what the drives support. */
#ifndef KINETIC_USE_TLS_1_2
#define KINETIC_USE_TLS_1_2 0
#endif

/** Initialize the SSL library internals for use by the messaging bus. */
bool BusSSL_Init(struct bus *b);

/** Do an SSL / TLS shake for a connection. Blocking. */
SSL *BusSSL_Connect(struct bus *b, int fd);

/** Disconnect and free an individual SSL handle. */
bool BusSSL_Disconnect(struct bus *b, SSL *ssl);

/** Free all internal data for using SSL (the SSL_CTX). */
void BusSSL_CtxFree(struct bus *b);

#endif
