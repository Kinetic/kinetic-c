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

KineticStatus KineticController_CreateWorkerThreads(KineticConnection* const connection)
{
    KineticStatus status;

    int pthreadStatus = pthread_create(
        &connection->threadID,
        NULL,
        KineticController_ReceiveThread,
        &connection->thread);
    if (pthreadStatus != 0) {
        char errMsg[256];
        Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
        LOGF0("Failed creating worker thread w/error: %s", errMsg);
        status = KINETIC_STATUS_CONNECTION_ERROR;
    }
    else {
        status = KINETIC_STATUS_SUCCESS;
    }

    return status;
}

KineticOperation* KineticController_CreateOperation(KineticSessionHandle handle)
{
    if (handle == KINETIC_HANDLE_INVALID) {
        LOG0("Specified session has invalid handle value");
        return NULL;
    }

    KineticConnection* connection = KineticConnection_FromHandle(handle);
    if (connection == NULL) {
        LOG0("Specified session is not associated with a connection");
        return NULL;
    }

    LOGF1("\n"
         "--------------------------------------------------\n"
         "Building new operation on connection @ 0x%llX", connection);

    KineticOperation* operation = KineticAllocator_NewOperation(connection);
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
    data->status = kinetic_data->status;
    pthread_cond_signal(&data->receiveComplete);
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

    KineticOperation_SetTimeoutTime(operation, KINETIC_OPERATION_TIMEOUT_SECS);

    if (closure != NULL)
    {
        operation->closure = *closure;
        return KineticOperation_SendRequest(operation);
    }
    else
    {
        DefaultCallbackData data;
        pthread_mutex_init(&data.receiveCompleteMutex, NULL);
        pthread_cond_init(&data.receiveComplete, NULL);
        data.status = KINETIC_STATUS_SUCCESS;

        operation->closure = DefaultClosure(&data);

        // Send the request
        status = KineticOperation_SendRequest(operation);

        if (status == KINETIC_STATUS_SUCCESS) {
            pthread_mutex_lock(&data.receiveCompleteMutex);
            pthread_cond_wait(&data.receiveComplete, &data.receiveCompleteMutex);
            pthread_mutex_unlock(&data.receiveCompleteMutex);
            status = data.status;
        }

        pthread_cond_destroy(&data.receiveComplete);
        pthread_mutex_destroy(&data.receiveCompleteMutex);

        return status;
    }
}

void KineticController_Pause(KineticConnection* const connection, bool pause)
{
    assert(connection != NULL);
    connection->thread.paused = pause;
}

void* KineticController_ReceiveThread(void* thread_arg)
{
    KineticStatus status;
    KineticThread* thread = thread_arg;

    while(!thread->abortRequested && !thread->fatalError) {

        // Do not service PDUs if thread paused
        if (thread->paused) {
            sleep(0);
            continue;
        } 

        // Wait for and receive a PDU
        KineticWaitStatus wait_status = KineticSocket_WaitUntilDataAvailable(thread->connection->socket, 100);
        switch(wait_status)
        {
            case KINETIC_WAIT_STATUS_DATA_AVAILABLE:
            {
                KineticPDU* response = KineticAllocator_NewPDU(thread->connection);
                status = KineticPDU_ReceiveMain(response);
                if (status != KINETIC_STATUS_SUCCESS) {
                    LOGF0("ERROR: PDU receive reported an error: %s", Kinetic_GetStatusDescription(status));
                }
                else {
                    status = KineticPDU_GetStatus(response);
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
                            LOG0("WARNING: Unsolicited PDU is not recognized!");
                        }
                        KineticAllocator_FreePDU(thread->connection, response);
                    }

                    // Associate solicited response PDUs with their requests
                    else {
                        response->type = KINETIC_PDU_TYPE_RESPONSE;
                        KineticOperation* op = KineticOperation_AssociateResponseWithOperation(response);
                        if (op == NULL) {
                            LOG0("Failed to find request matching received response PDU!");
                            KineticAllocator_FreePDU(thread->connection, response);
                        }
                        else {
                            LOG2("Found associated operation/request for response PDU.");
                            size_t valueLength = KineticPDU_GetValueLength(response);
                            if (valueLength > 0) {
                                status = KineticPDU_ReceiveValue(op->connection->socket,
                                    &op->entry->value, valueLength);
                            }

                            // Call operation-specific callback, if configured
                            if (status == KINETIC_STATUS_SUCCESS && op->callback != NULL) {
                                status = op->callback(op);
                            }

                            // TODO what to use for status?  response? or op->response? adsfjklasdfjklsdafjkldfsaj 

                            if (status == KINETIC_STATUS_SUCCESS) {
                                status = KineticPDU_GetStatus(response);
                            }

                            KineticOperation_Complete(op, status);
                        }
                    }
                }
                else {
                    // Free invalid PDU
                    KineticAllocator_FreePDU(thread->connection, response);
                }
            } break;
            case KINETIC_WAIT_STATUS_TIMED_OUT:
            case KINETIC_WAIT_STATUS_RETRYABLE_ERROR:
            {
                sleep(0);
            } break;
            default:
            case KINETIC_WAIT_STATUS_FATAL_ERROR:
            {
                LOG0("ERROR: Socket error while waiting for PDU to arrive");
                thread->fatalError = true;
            } break;
        }

        KineticOperation_TimeoutOperations(thread->connection);
    }

    LOG1("Worker thread terminated!");
    return (void*)NULL;
}
