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

#include "kinetic_operation.h"
#include "kinetic_controller.h"
#include "kinetic_session.h"
#include "kinetic_message.h"
#include "kinetic_bus.h"
#include "kinetic_response.h"
#include "kinetic_device_info.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic_request.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "kinetic_acl.h"

#ifdef TEST
uint8_t * msg = NULL;
size_t msgSize = 0;
#endif

void KineticOperation_ValidateOperation(KineticOperation* op)
{
    KINETIC_ASSERT(op);
    KINETIC_ASSERT(op->session);
    KINETIC_ASSERT(op->request);
    KINETIC_ASSERT(op->request->command);
    KINETIC_ASSERT(op->request->command->header);
    KINETIC_ASSERT(op->request->command->header->has_sequence);
}

static KineticStatus send_request_in_lock(KineticOperation* const op);

KineticStatus KineticOperation_SendRequest(KineticOperation* const op)
{
    KineticSession *session = op->session;
    KineticOperation_ValidateOperation(op);

    if (!KineticRequest_LockSend(session)) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }
    KineticStatus status = send_request_in_lock(op);
    KineticRequest_UnlockSend(session);
    return status;
}

static void log_request_seq_id(int fd, int64_t seq_id, KineticMessageType mt)
{
    #ifdef TEST
    (void)fd;
    (void)seq_id;
    (void)mt;
    #else
    #if KINETIC_LOGGER_LOG_SEQUENCE_ID
    struct timeval tv;
    gettimeofday(&tv, NULL);
    LOGF2("SEQ_ID request fd %d seq_id %lld %08lld.%08lld cmd %02x",
        fd, (long long)seq_id,
        (long long)tv.tv_sec, (long long)tv.tv_usec, (uint8_t)mt);
    #else
    (void)seq_id;
    (void)mt;
    #endif
    #endif
}

/* Send request.
 * Note: This whole function operates with op->session->sendMutex locked. */
static KineticStatus send_request_in_lock(KineticOperation* const op)
{
    LOGF3("\nSending PDU via fd=%d", op->session->socket);
    KineticRequest* request = op->request;

    int64_t seq_id = KineticSession_GetNextSequenceCount(op->session);
    KINETIC_ASSERT(request->message.header.sequence == KINETIC_SEQUENCE_NOT_YET_BOUND);
    request->message.header.sequence = seq_id;

    size_t expectedLen = KineticRequest_PackCommand(request);
    if (expectedLen == KINETIC_REQUEST_PACK_FAILURE) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    uint8_t * commandData = request->message.message.commandbytes.data;

    log_request_seq_id(op->session->socket, seq_id, (KineticMessageType)request->message.header.messagetype);

    KineticSession *session = op->session;
    KineticStatus status = KineticRequest_PopulateAuthentication(&session->config,
        op->request, op->pin);
    if (status != KINETIC_STATUS_SUCCESS) {
        if (commandData) { free(commandData); }
        return status;
    }

    #ifndef TEST
    uint8_t * msg = NULL;
    size_t msgSize = 0;
    #endif
    status = KineticRequest_PackMessage(op, &msg, &msgSize);
    if (status != KINETIC_STATUS_SUCCESS) {
        if (commandData) { free(commandData); }
        return status;
    }

    if (commandData) { free(commandData); }
    KineticCountingSemaphore * const sem = op->session->outstandingOperations;
    KineticCountingSemaphore_Take(sem);  // limit total concurrent requests

    if (!KineticRequest_SendRequest(op, msg, msgSize)) {
        LOGF0("Failed queuing request %p for transmit on fd=%d w/seq=%lld",
            (void*)request, op->session->socket, (long long)seq_id);
        /* A false result from bus_send_request means that the request was
         * rejected outright, so the usual asynchronous, callback-based
         * error handling for errors during the request or response will
         * not be used. */
        KineticCountingSemaphore_Give(sem);
        status = KINETIC_STATUS_REQUEST_REJECTED;
    } else {
        status = KINETIC_STATUS_SUCCESS;
    }

    if (msg != NULL) { free(msg); }
    return status;
}

KineticStatus KineticOperation_GetStatus(const KineticOperation* const op)
{
    KineticStatus status = KINETIC_STATUS_INVALID;
    if (op != NULL) {
        status = KineticResponse_GetStatus(op->response);
    }
    return status;
}

void KineticOperation_Complete(KineticOperation* op, KineticStatus status)
{
    KINETIC_ASSERT(op);
    KINETIC_ASSERT(op->session);

    // ExecuteOperation should ensure a callback exists (either a user supplied one, or the a default)
    KineticCompletionData completionData = {.status = status};

    // Release this request so that others can be unblocked if at max (request PDUs throttled)
    KineticCountingSemaphore_Give(op->session->outstandingOperations);

    if(op->closure.callback != NULL) {
        op->closure.callback(&completionData, op->closure.clientData);
    }

    KineticAllocator_FreeOperation(op);
}
