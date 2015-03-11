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
#ifndef BUS_INWARD_H
#define BUS_INWARD_H

#include "bus_types.h"

/* Get the string key for a log event ID. */
const char *Bus_LogEventStr(log_event_t event);

/* For a given file descriptor, get the listener ID to use.
 * This will level sockets between multiple threads. */
struct listener *Bus_GetListenerForSocket(struct bus *b, int fd);

/* Lock / unlock the log mutex, since logging can occur on several threads. */
void Bus_LockLog(struct bus *b);
void Bus_UnlockLog(struct bus *b);

/* Deliver a boxed message to the thread pool to execute. */
bool Bus_ProcessBoxedMessage(struct bus *b,
    struct boxed_msg *box, size_t *backpressure);

/* Provide backpressure by sleeping for (backpressure >> shift) msec, if
 * the value is greater than 0. */
void Bus_BackpressureDelay(struct bus *b, size_t backpressure, uint8_t shift);

#endif
