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

#ifndef _KINETIC_SOCKET_H
#define _KINETIC_SOCKET_H

#include "kinetic_types_internal.h"
#include "kinetic_message.h"

typedef enum
{
    KINETIC_WAIT_STATUS_DATA_AVAILABLE,
    KINETIC_WAIT_STATUS_TIMED_OUT,
    KINETIC_WAIT_STATUS_RETRYABLE_ERROR,
    KINETIC_WAIT_STATUS_FATAL_ERROR,
} KineticWaitStatus;

int KineticSocket_Connect(const char* host, int port);
void KineticSocket_Close(int socket);

void KineticSocket_BeginPacket(int socket);
void KineticSocket_FinishPacket(int socket);
void KineticSocket_EnableTCPNoDelay(int socket);

#endif // _KINETIC_SOCKET_H
