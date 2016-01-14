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

#ifndef LISTENER_TASK_H
#define LISTENER_TASK_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener_internal_types.h"

/** Listener's main loop -- function pointer for pthread start function. */
void *ListenerTask_MainLoop(void *arg);

/** Release a message to the listener's message pool. */
void ListenerTask_ReleaseMsg(listener *l, listener_msg *msg);

/** Release an INFO to the listener's info pool. */
void ListenerTask_ReleaseRXInfo(listener *l, struct rx_info_t *info);

/** Grow the listener's read buffer to NSIZE. */
bool ListenerTask_GrowReadBuf(listener *l, size_t nsize);

/** Attempt delivery of the message boxed in INFO. */
void ListenerTask_AttemptDelivery(listener *l, struct rx_info_t *info);

/** Notify the client that the event in INFO has failed with STATUS. */
void ListenerTask_NotifyMessageFailure(listener *l,
    rx_info_t *info, bus_send_status_t status);

/** Get the current backpressure from the listener. */
uint16_t ListenerTask_GetBackpressure(struct listener *l);

/** Dump the RX info table. (Debugging only.) */
void ListenerTask_DumpRXInfoTable(listener *l);

#endif

