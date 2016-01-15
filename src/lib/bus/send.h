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

#ifndef SEND_H
#define SEND_H

#include "bus_types.h"
#include "bus_internal_types.h"

/** Do a blocking send.
 *
 * Returning true indicates that the message has been queued up for
 * delivery, but the request or response may still fail. Those errors
 * are handled by giving an error status code to the callback.
 * Returning false means that the send was rejected outright, and
 * the callback-based error handling will not be used. */
bool Send_DoBlockingSend(struct bus *b, boxed_msg *box);

void Send_HandleFailure(struct bus *b, boxed_msg *box, bus_send_status_t status);

#endif
