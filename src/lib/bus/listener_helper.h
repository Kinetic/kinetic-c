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

#ifndef LISTENER_HELPER_H
#define LISTENER_HELPER_H

#include "bus_internal_types.h"
#include "listener.h"
#include "listener_internal_types.h"

/** Get a free message from the listener's message pool. */
listener_msg *ListenerHelper_GetFreeMsg(listener *l);

/** Push a message into the listener's message queue. */
bool ListenerHelper_PushMessage(struct listener *l, listener_msg *msg, int *reply_fd);

/** Get a free RX_INFO record, if any are available. */
rx_info_t *ListenerHelper_GetFreeRXInfo(listener *l);

/** Try to find an RX_INFO record by a <file descriptor, sequence_id> pair. */
rx_info_t *ListenerHelper_FindInfoBySequenceID(listener *l,
    int fd, int64_t seq_id);

#endif
