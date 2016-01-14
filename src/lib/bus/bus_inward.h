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

#ifndef BUS_INWARD_H
#define BUS_INWARD_H

#include "bus_types.h"

/** Get the string key for a log event ID. */
const char *Bus_LogEventStr(log_event_t event);

/** For a given file descriptor, get the listener ID to use.
 * This will level sockets between multiple threads. */
struct listener *Bus_GetListenerForSocket(struct bus *b, int fd);

/** Deliver a boxed message to the thread pool to execute. */
bool Bus_ProcessBoxedMessage(struct bus *b,
    struct boxed_msg *box, size_t *backpressure);

/** Provide backpressure by sleeping for (backpressure >> shift) msec, if
 * the value is greater than 0. */
void Bus_BackpressureDelay(struct bus *b, size_t backpressure, uint8_t shift);

#endif
