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

#include "kinetic_connection.h"
#include "kinetic_types_internal.h"
#include "kinetic_socket.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

STATIC KineticConnection ConnectionInstances[KINETIC_SESSIONS_MAX];
STATIC KineticConnection* Connections[KINETIC_SESSIONS_MAX];


void KineticConnection_Pause(KineticConnection* const connection, bool pause)
{
    assert(connection != NULL);
    connection->thread.paused = pause;
}

static void* KineticConnection_Worker(void* thread_arg)
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

                            // Call client-supplied closure callback, if supplied
                            if (op->closure.callback != NULL) {
                                KineticCompletionData completionData = {.status = status};
                                op->closure.callback(&completionData, op->closure.clientData);
                                KineticAllocator_FreeOperation(thread->connection, op);
                            }

                            // Otherwise, is a synchronous opearation, so just set a flag
                            else {
                                op->receiveComplete = true;
                            }
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
    }

    LOG1("Worker thread terminated!");
    return (void*)NULL;
}

KineticSessionHandle KineticConnection_NewConnection(
    const KineticSession* const config)
{
    KineticSessionHandle handle = KINETIC_HANDLE_INVALID;
    if (config == NULL) {
        return KINETIC_HANDLE_INVALID;
    }
    for (int idx = 0; idx < KINETIC_SESSIONS_MAX; idx++) {
        if (Connections[idx] == NULL) {
            KineticConnection* connection = &ConnectionInstances[idx];
            Connections[idx] = connection;
            KINETIC_CONNECTION_INIT(connection);
            connection->session = *config;
            handle = (KineticSessionHandle)(idx + 1);
            return handle;
        }
    }
    return KINETIC_HANDLE_INVALID;
}

void KineticConnection_FreeConnection(KineticSessionHandle* const handle)
{
    assert(handle != NULL);
    assert(*handle != KINETIC_HANDLE_INVALID);
    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    assert(connection != NULL);
    *connection = (KineticConnection) {
        .connected = false
    };
    Connections[(int)*handle - 1] = NULL;
}

KineticConnection* KineticConnection_FromHandle(KineticSessionHandle handle)
{
    assert(handle > KINETIC_HANDLE_INVALID);
    assert(handle <= KINETIC_SESSIONS_MAX);
    return Connections[(int)handle - 1];
}

KineticStatus KineticConnection_Connect(KineticConnection* const connection)
{
    if (connection == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    // Establish the connection
    connection->connected = false;
    connection->socket = KineticSocket_Connect(
                             connection->session.host,
                             connection->session.port,
                             connection->session.nonBlocking);
    connection->connected = (connection->socket >= 0);
    if (!connection->connected) {
        LOG0("Session connection failed!");
        connection->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Kick off the worker thread
    connection->thread.connection = connection;
    int pthreadStatus = pthread_create(&connection->threadID, NULL, KineticConnection_Worker, &connection->thread);
    if (pthreadStatus != 0) {
        char errMsg[256];
        Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
        LOGF0("Failed creating worker thread w/error: %s", errMsg);
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticConnection_Disconnect(KineticConnection* const connection)
{
    if (connection == NULL || !connection->connected || connection->socket < 0) {
        return KINETIC_STATUS_SESSION_INVALID;
    }

    // Shutdown the worker thread
    KineticStatus status = KINETIC_STATUS_SUCCESS;
    connection->thread.abortRequested = true;
    LOG2("\nSent abort request to worker thread!\n");
    int pthreadStatus = pthread_join(connection->threadID, NULL);
    if (pthreadStatus != 0) {
        char errMsg[256];
        Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
        LOGF0("Failed terminating worker thread w/error: %s", errMsg);
        status = KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Close the connection
    close(connection->socket);
    connection->socket = KINETIC_HANDLE_INVALID;
    connection->connected = false;

    return status;
}

void KineticConnection_IncrementSequence(KineticConnection* const connection)
{
    assert(connection != NULL);
    connection->sequence++;
}
