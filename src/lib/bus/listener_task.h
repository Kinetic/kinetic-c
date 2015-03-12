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

