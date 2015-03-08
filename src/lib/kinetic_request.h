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
#ifndef KINETIC_REQUEST_H
#define KINETIC_REQUEST_H

#include "kinetic_types_internal.h"

/* Special 'failed to pack' sentinel. */
#define KINETIC_REQUEST_PACK_FAILURE ((size_t)-1)

/* Pack the command into request->message.message.commandBytes.data.
 * Returns the allocated size,
 * or KINETIC_REQUEST_PACK_FAILURE on malloc failure. */
size_t KineticRequest_PackCommand(KineticRequest* request);

/* Populate the request's authentication info. If PIN is non-NULL,
 * use PIN authentication, otherwise use HMAC. */
KineticStatus KineticRequest_PopulateAuthentication(KineticSessionConfig *config,
    KineticRequest *request, ByteArray *pin);

/* Pack the header, command, and value (if any), allocating a buffer and
 * returning the buffer and its size in *msg and *msgSize.
 * Returns KINETIC_STATUS_SUCCESS on success, or KINETIC_STATUS_MEMORY_ERROR
 * on allocation failure. */
KineticStatus KineticRequest_PackMessage(KineticOperation *operation,
    uint8_t **msg, size_t *msgSize);

/* Send the request. Returns whether the request was successfully queued
 * up for delivery, or whether it was rejected due to invalid arguments.
 * If this returns false, then the asynchronous result callback will
 * not be called. */
bool KineticRequest_SendRequest(KineticOperation *operation,
    uint8_t *msg, size_t msgSize);

bool KineticRequest_LockConnection(KineticSession* session);
bool KineticRequest_UnlockConnection(KineticSession* session);

#endif
