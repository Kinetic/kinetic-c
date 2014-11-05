/*
* kinetic-c
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

int KineticSocket_Connect(const char* host, int port, bool nonBlocking);
void KineticSocket_Close(int socket);

int KineticSocket_DataBytesAvailable(int socket);
KineticWaitStatus KineticSocket_WaitUntilDataAvailable(int socket, int timeout);
KineticStatus KineticSocket_Read(int socket, ByteBuffer* dest, size_t len);
KineticStatus KineticSocket_ReadProtobuf(int socket, KineticPDU* pdu);

KineticStatus KineticSocket_Write(int socket, ByteBuffer* src);
KineticStatus KineticSocket_WriteProtobuf(int socket, KineticPDU* pdu);

#endif // _KINETIC_SOCKET_H
