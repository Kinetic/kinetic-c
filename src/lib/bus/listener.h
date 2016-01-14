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

#ifndef LISTENER_H
#define LISTENER_H

#include "bus_types.h"
#include "bus_internal_types.h"

/** How many bits to >> the backpressure value from commands
 * delivered to the listener. */
#define LISTENER_BACKPRESSURE_SHIFT 0 /* TODO */

/** How many bits to >> the backpressure value from the listener when a
 * send has completed. */
#define LISTENER_EXPECT_BACKPRESSURE_SHIFT 7

/** Manager of incoming messages from drives, both responses and
 * unsolicited status updates. */
struct listener;

/** Initialize the listener. */
struct listener *Listener_Init(struct bus *b, struct bus_config *cfg);

/** Add/remove sockets' metadata from internal info. Blocking. */
bool Listener_AddSocket(struct listener *l, connection_info *ci, int *notify_fd);
bool Listener_RemoveSocket(struct listener *l, int fd, int *notify_fd);

/** The client is about to start a write, the listener should hold on to
 * the response (with timeout) if it arrives before receiving further
 * instructions from the client. */
bool Listener_HoldResponse(struct listener *l, int fd,
    int64_t seq_id, int16_t timeout_sec, int *notify_fd);

/** The client has finished a write, the listener should expect a response. */
bool Listener_ExpectResponse(struct listener *l, boxed_msg *box,
    uint16_t *backpressure);

/** Shut down the listener. Blocking. */
bool Listener_Shutdown(struct listener *l, int *notify_fd);

/** Free the listener, which must already be shut down. */
void Listener_Free(struct listener *l);

#ifdef TEST
#include "listener_internal_types.h"
#endif

#endif
