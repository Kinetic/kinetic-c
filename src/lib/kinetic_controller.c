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

#include "kinetic_controller.h"
#include "kinetic_session.h"
#include "kinetic_operation.h"
#include "kinetic_bus.h"
#include "kinetic_response.h"
#include "kinetic_auth.h"
#include "kinetic_socket.h"
#include "kinetic_allocator.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_logger.h"
#include <pthread.h>
#include "bus.h"

typedef struct {
    pthread_mutex_t receiveCompleteMutex;
    pthread_cond_t receiveComplete;
    bool completed;
    KineticStatus status;
} DefaultCallbackData;

static void DefaultCallback(KineticCompletionData* kinetic_data, void* client_data)
{
    DefaultCallbackData * data = client_data;
    pthread_mutex_lock(&data->receiveCompleteMutex);
    data->status = kinetic_data->status;
    data->completed = true;
    pthread_cond_signal(&data->receiveComplete);
    pthread_mutex_unlock(&data->receiveCompleteMutex);
}

STATIC KineticCompletionClosure DefaultClosure(DefaultCallbackData * const data)
{
    return (KineticCompletionClosure) {
        .callback = DefaultCallback,
        .clientData = data,
    };
}

KineticStatus KineticController_ExecuteOperation(KineticOperation* operation, KineticCompletionClosure* const closure)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KineticStatus status = KINETIC_STATUS_INVALID;
    KineticSession *session = operation->session;

    if (KineticSession_GetTerminationStatus(operation->session) != KINETIC_STATUS_SUCCESS) {
        return KINETIC_STATUS_SESSION_TERMINATED;
    }

    if (closure != NULL) {
        operation->closure = *closure;
        return KineticOperation_SendRequest(operation);
    }
    else {
        DefaultCallbackData data;
        pthread_mutex_init(&data.receiveCompleteMutex, NULL);
        pthread_cond_init(&data.receiveComplete, NULL);
        data.status = KINETIC_STATUS_INVALID;
        data.completed = false;

        operation->closure = DefaultClosure(&data);

        // Send the request
        status = KineticOperation_SendRequest(operation);

        if (status == KINETIC_STATUS_SUCCESS) {
            pthread_mutex_lock(&data.receiveCompleteMutex);
            while(data.completed == false) {
                pthread_cond_wait(&data.receiveComplete, &data.receiveCompleteMutex);
            }
            status = data.status;
            pthread_mutex_unlock(&data.receiveCompleteMutex);
        }

        pthread_cond_destroy(&data.receiveComplete);
        pthread_mutex_destroy(&data.receiveCompleteMutex);

        if (status != KINETIC_STATUS_SUCCESS) {
            if (KineticSession_GetTerminationStatus(session) != KINETIC_STATUS_SUCCESS) {
                (void)KineticSession_Disconnect(session);
                if (status == KINETIC_STATUS_SOCKET_ERROR) {
                    status = KINETIC_STATUS_SESSION_TERMINATED;
                }
            }
        }

        return status;
    }
}

KineticStatus bus_to_kinetic_status(bus_send_status_t const status)
{
    KineticStatus res = KINETIC_STATUS_INVALID;

    switch(status) {
        // TODO scrutinize all these mappings
        case BUS_SEND_SUCCESS:
            res = KINETIC_STATUS_SUCCESS;
            break;
        case BUS_SEND_TX_TIMEOUT:
            res = KINETIC_STATUS_SOCKET_TIMEOUT;
            break;
        case BUS_SEND_TX_FAILURE:
            res = KINETIC_STATUS_SOCKET_ERROR;
            break;
        case BUS_SEND_RX_TIMEOUT:
            res = KINETIC_STATUS_OPERATION_TIMEDOUT;
            break;
        case BUS_SEND_RX_FAILURE:
            res = KINETIC_STATUS_SOCKET_ERROR;
            break;
        case BUS_SEND_BAD_RESPONSE:
            res = KINETIC_STATUS_SOCKET_ERROR;
            break;
        case BUS_SEND_UNREGISTERED_SOCKET:
            res = KINETIC_STATUS_SOCKET_ERROR;
            break;
        case BUS_SEND_RX_TIMEOUT_EXPECT:
            res = KINETIC_STATUS_OPERATION_TIMEDOUT;
            break;
        case BUS_SEND_UNDEFINED:
        default:
        {
            LOGF0("bus_to_kinetic_status: UNMATCHED %d", status);
            KINETIC_ASSERT(false);
            return KINETIC_STATUS_INVALID;
        }
    }

    LOGF3("bus_to_kinetic_status: mapping status %d => %d",
        status, res);
    return res;
}

static const char *bus_error_string(bus_send_status_t t) {
    switch (t) {
    default:
    case BUS_SEND_UNDEFINED:
        return "undefined";
    case BUS_SEND_SUCCESS:
        return "success";
    case BUS_SEND_TX_TIMEOUT:
        return "tx_timeout";
    case BUS_SEND_TX_FAILURE:
        return "tx_failure";
    case BUS_SEND_RX_TIMEOUT:
        return "rx_timeout";
    case BUS_SEND_RX_FAILURE:
        return "rx_failure";
    case BUS_SEND_BAD_RESPONSE:
        return "bad_response";
    case BUS_SEND_UNREGISTERED_SOCKET:
        return "unregistered_socket";
    case BUS_SEND_RX_TIMEOUT_EXPECT:
        return "internal_timeout";
    case BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER:
        return "notifying_timeout";
    case BUS_SEND_TIMESTAMP_ERROR:
        return "timestamp_error";
    }
}

void KineticController_HandleUnexpectedResponse(void *msg,
                                                int64_t seq_id,
                                                void *bus_udata,
                                                void *socket_udata)
{
    KineticResponse * response = msg;
    KineticSession* session = socket_udata;
    bool connetionInfoReceived = false;
    char const * statusTag = "[PDU RX STATUS]";
    char const * unexpectedTag = "[PDU RX UNEXPECTED]";
    char const * logTag = unexpectedTag;
    int logAtLevel, protoLogAtLevel;

    (void)bus_udata;

    // Handle unsolicited status PDUs
    if (response->proto->authtype == COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__UNSOLICITEDSTATUS) {
        int64_t connectionID = KineticResponse_GetConnectionID(response);
        if (connectionID != 0)
        {
            // Store connectionID from unsolicited status message in the session for future requests
            KineticSession_SetConnectionID(session, connectionID);
            LOGF2("Extracted connection ID from unsolicited status PDU (id=%lld)", connectionID);
            connetionInfoReceived = true;
            logTag = statusTag;
            logAtLevel = 2;
            protoLogAtLevel = 3;
        }
        else {
            LOG0("WARNING: Unsolicited status received. Connection being terminated by remote!");
            logTag = statusTag;
            logAtLevel = 0;
            protoLogAtLevel = 0;
            KineticStatus status = KineticResponse_GetStatus(response);
            KineticSession_SetTerminationStatus(session, status);
        }
    }
    else {
        LOG0("WARNING: Received unexpected response!");
        logTag = unexpectedTag;
        logAtLevel = 0;
        protoLogAtLevel = 0;
    }

    KineticLogger_LogPrintf(logAtLevel, "%s pdu: %p, session: %p, bus: %p, "
        "fd: %6d, seq: %8lld, protoLen: %8u, valueLen: %8u",
        logTag,
        (void*)response, (void*)session,
        (void*)session->messageBus,
        session->socket, (long long)seq_id,
        KineticResponse_GetProtobufLength(response),
        KineticResponse_GetValueLength(response));
    KineticLogger_LogProtobuf(protoLogAtLevel, response->proto);

    KineticAllocator_FreeKineticResponse(response);

    if (connetionInfoReceived) {
        KineticResourceWaiter_SetAvailable(&session->connectionReady);
    }
}

void KineticController_HandleResult(bus_msg_result_t *res, void *udata)
{
    KineticOperation* op = udata;
    KINETIC_ASSERT(op);
    KINETIC_ASSERT(op->session);

    KineticStatus status = bus_to_kinetic_status(res->status);

    if (status == KINETIC_STATUS_SUCCESS) {
        KineticResponse * response = res->u.response.opaque_msg;

        status = KineticResponse_GetStatus(response);

        LOGF2("[PDU RX] pdu: %p, session: %p, bus: %p, "
            "fd: %6d, seq: %8lld, protoLen: %8u, valueLen: %8u, op: %p, status: %s",
            (void*)response,
            (void*)op->session, (void*)op->session->messageBus,
            op->session->socket, response->command->header->acksequence,
            KineticResponse_GetProtobufLength(response),
            KineticResponse_GetValueLength(response),
            (void*)op,
            Kinetic_GetStatusDescription(status));
        KineticLogger_LogHeader(3, &response->header);
        KineticLogger_LogProtobuf(3, response->proto);

        if (op->response == NULL) {
            op->response = response;
        }
    } else {
        LOGF0("Error receiving response, got message bus error: %s", bus_error_string(res->status));
        if (res->status == BUS_SEND_RX_TIMEOUT) {
            LOG0("RX_TIMEOUT");
        }
    }

    // Call operation-specific callback, if configured
    if (op->opCallback != NULL) {
        status = op->opCallback(op, status);
    }

    KineticOperation_Complete(op, status);
}

