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

#include "kinetic_controller.h"
#include "kinetic_connection.h"
#include "kinetic_operation.h"
#include "kinetic_pdu.h"
#include "kinetic_socket.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <pthread.h>
#include <malloc.h>

KineticStatus KineticController_Init(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    int pthreadStatus = pthread_create(
        &session->connection->threadID,
        NULL,
        KineticController_ReceiveThread,
        &session->connection->thread);
    if (pthreadStatus != 0) {
        char errMsg[256];
        Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
        LOGF0("Failed creating worker thread w/error: %s", errMsg);
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticOperation* KineticController_CreateOperation(KineticSession const * const session)
{
    if (session == NULL) {
        LOG0("Specified session is NULL");
        return NULL;
    }

    if (session->connection == NULL) {
        LOG0("Specified session is not associated with a connection");
        return NULL;
    }

    LOGF1("\n"
         "--------------------------------------------------\n"
         "Building new operation on session @ 0x%llX", session);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL || operation->request == NULL) {
        return NULL;
    }

    return operation;
}

typedef struct {
    pthread_mutex_t receiveCompleteMutex;
    pthread_cond_t receiveComplete;
    KineticStatus status;
} DefaultCallbackData;

static void DefaultCallback(KineticCompletionData* kinetic_data, void* client_data)
{
    DefaultCallbackData * data = client_data;
    LOGF1("  Callback invoked for (0x%0llX) ", data);
    data->status = kinetic_data->status;
    pthread_mutex_lock(&data->receiveCompleteMutex);
    pthread_cond_signal(&data->receiveComplete);
    pthread_mutex_unlock(&data->receiveCompleteMutex);
}

static KineticCompletionClosure DefaultClosure(DefaultCallbackData * const data)
{
    return (KineticCompletionClosure) {
        .callback = DefaultCallback,
        .clientData = data,
    };
}

KineticStatus KineticController_ExecuteOperation(KineticOperation* operation, KineticCompletionClosure* const closure)
{
    assert(operation != NULL);
    KineticStatus status = KINETIC_STATUS_INVALID;

    LOGF1("Executing operation: 0x%llX", operation);
    if (operation->entry != NULL &&
        operation->entry->value.array.data != NULL &&
        operation->entry->value.bytesUsed > 0)
    {
        LOGF1("  Sending PDU (0x%0llX) w/value (%zu bytes)",
            operation->request, operation->entry->value.bytesUsed);
    }
    else {
        LOGF1("  Sending PDU (0x%0llX) w/o value", operation->request);
    }

    if (closure != NULL)
    {
        operation->closure = *closure;
        return KineticOperation_SendRequest(operation);
    }
    else
    {
        DefaultCallbackData *data = malloc(sizeof(*data)) ;
	assert(data);
        pthread_mutex_init(&data->receiveCompleteMutex, NULL);
        pthread_cond_init(&data->receiveComplete, NULL);
        data->status = KINETIC_STATUS_INVALID;

        operation->closure.callback = DefaultCallback;
        operation->closure.clientData = data;

        // Send the request
        pthread_mutex_lock(&data->receiveCompleteMutex);
        status = KineticOperation_SendRequest(operation);

        if (status == KINETIC_STATUS_SUCCESS) {
	    LOGF1("  Waiting for completion of operation (0x%0llX) ", operation);
            pthread_cond_wait(&data->receiveComplete, &data->receiveCompleteMutex);
            status = data->status;
	    LOGF1("  Wait ended for operation (0x%0llX) ", operation);
        }
        pthread_mutex_unlock(&data->receiveCompleteMutex);

        pthread_cond_destroy(&data->receiveComplete);
        pthread_mutex_destroy(&data->receiveCompleteMutex);
	free(data);

        return status;
    }
}

void KineticController_Pause(KineticSession const * const session, bool pause)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    session->connection->thread.paused = pause;
}

void KineticController_HandleIncomingPDU(KineticConnection* const connection)
{
    KineticPDU* response = KineticAllocator_NewPDU(connection);
    KineticStatus status = KineticPDU_ReceiveMain(response);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF0("ERROR: PDU receive reported an error: %s",
            Kinetic_GetStatusDescription(status));
    }

    if (response->proto != NULL && response->proto->has_authType) {

        // Handle unsolicited status PDUs
        if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS) {
            response->type = KINETIC_PDU_TYPE_UNSOLICITED;
            if (response->command != NULL &&
                response->command->header != NULL &&
                response->command->header->has_connectionID)
            {
                // Extract connectionID from unsolicited status message
                response->connection->connectionID = response->command->header->connectionID;
                LOGF2("Extracted connection ID from unsolicited status PDU (id=%lld)",
                    response->connection->connectionID);
            }
            else {
                LOG0("WARNING: Unsolicited PDU in invalid. Does not specify connection ID!");
            }
            KineticAllocator_FreePDU(connection, response);
        }

        // Associate solicited response PDUs with their requests
        else {
            response->type = KINETIC_PDU_TYPE_RESPONSE;
            KineticOperation* op = KineticOperation_AssociateResponseWithOperation(response);
            if (op == NULL) {
                LOG0("Failed to find request matching received response PDU!");
                KineticAllocator_FreePDU(connection, response);
            }
            else {
                LOG2("Found associated operation/request for response PDU.");
                size_t valueLength = KineticPDU_GetValueLength(response);
                if (valueLength > 0) {
                    // we need to try to read the value off the socket, even in the case of an error
                    //  otherwise the stream will get out of sync
                    KineticStatus valueStatus = KineticPDU_ReceiveValue(op->connection->socket,
                        &op->entry->value, valueLength);

                    // so we only care about the status of the value read if the operation
                    //   succeeded 
                    if (status == KINETIC_STATUS_SUCCESS) {
                        status = valueStatus;
                    }
                }

                // Call operation-specific callback, if configured
                if (status == KINETIC_STATUS_SUCCESS && op->callback != NULL) {
                    status = op->callback(op, KINETIC_STATUS_SUCCESS);
                }

                if (status == KINETIC_STATUS_SUCCESS) {
                    status = KineticPDU_GetStatus(response);
                }
                
                LOGF2("Response PDU received w/status %s, %i",
                    Kinetic_GetStatusDescription(status), status);

                KineticOperation_Complete(op, status);
            }
        }
    }
    else {
        // Free invalid PDU
        KineticAllocator_FreePDU(connection, response);
    }
}

static KineticStatus service_controller(KineticConnection* const connection)
{
    // Wait for and receive a PDU
    KineticWaitStatus wait_status = KineticSocket_WaitUntilDataAvailable(connection->socket, 100);
    switch(wait_status)
    {
        case KINETIC_WAIT_STATUS_DATA_AVAILABLE:
        {
            KineticController_HandleIncomingPDU(connection);
        } break;
        case KINETIC_WAIT_STATUS_TIMED_OUT:
        case KINETIC_WAIT_STATUS_RETRYABLE_ERROR:
            break; // Break upon rcoverable/retryable conditions in order to retry 
        default:
        case KINETIC_WAIT_STATUS_FATAL_ERROR:
        {
            LOG0("ERROR: Socket error while waiting for PDU to arrive");
            return KINETIC_STATUS_SOCKET_ERROR;
        } break;
    }

    KineticOperation_TimeoutOperations(connection);

    return KINETIC_STATUS_SUCCESS;
}

void* KineticController_ReceiveThread(void* thread_arg)
{
    KineticThread* thread = thread_arg;

    while(!thread->abortRequested && !thread->fatalError) {

        // Do not service PDUs if thread paused
        if (thread->paused) {
            struct timespec sleepDuration = {.tv_nsec = 500000};
            nanosleep(&sleepDuration, NULL);
            continue;
        } 

        if (service_controller(thread->connection) != KINETIC_STATUS_SUCCESS) {
            thread->fatalError = true;
        }
    }

    LOG3("Worker thread terminated!");
    return (void*)NULL;
}
