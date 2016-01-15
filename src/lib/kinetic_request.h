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

bool KineticRequest_LockSend(KineticSession* session);
bool KineticRequest_UnlockSend(KineticSession* session);

#endif
