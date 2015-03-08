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

#include "kinetic_session.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_socket.h"
#include "kinetic_response.h"
#include "kinetic_operation.h"
#include "kinetic_controller.h"
#include "kinetic_allocator.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

KineticStatus KineticSession_Create(KineticSession * const session, KineticClient * const client)
{
    if (session == NULL) {
        LOG0("Session is NULL");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (client == NULL) {
        LOG0("Client is NULL");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    session->connected = false;
    session->socket = -1;
    
    // initialize session send mutex
    if (pthread_mutex_init(&session->sendMutex, NULL) != 0) {
        LOG0("Failed initializing session send mutex!");
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    session->outstandingOperations =
        KineticCountingSemaphore_Create(KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION);
    if (session->outstandingOperations == NULL) {
        LOG0("Failed creating session counting semaphore!");
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Destroy(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    KineticCountingSemaphore_Destroy(session->outstandingOperations);
    KineticAllocator_FreeSession(session);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Connect(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    // Establish the connection
    KINETIC_ASSERT(strlen(session->config.host) > 0);
    session->socket = KineticSocket_Connect(
        session->config.host, session->config.port);
    if (session->socket == KINETIC_SOCKET_DESCRIPTOR_INVALID) {
        LOG0("Session connection failed!");
        session->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
        session->connected = false;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }
    session->connected = true;

    bus_socket_t socket_type = session->config.useSsl ? BUS_SOCKET_SSL : BUS_SOCKET_PLAIN;
    session->si = calloc(1, sizeof(socket_info) + 2 * PDU_PROTO_MAX_LEN);
    if (session->si == NULL) { return KINETIC_STATUS_MEMORY_ERROR; }
    bool success = bus_register_socket(session->messageBus, socket_type, session->socket, session);
    if (!success) {
        LOG0("Failed registering connection with client!");
        goto connection_error_cleanup;
    }

    // Wait for initial unsolicited status to be received in order to obtain connection ID
    success = KineticResourceWaiter_WaitTilAvailable(&session->connectionReady, KINETIC_CONNECTION_TIMEOUT_SECS);
    if (!success) {
        LOG0("Timed out waiting for connection ID from device!");
        goto connection_error_cleanup;
    }
    LOGF1("Received connection ID %lld for session %p",
        (long long)session->connectionID, (void*)session);

    return KINETIC_STATUS_SUCCESS;

connection_error_cleanup:

    if (session->si != NULL) {
        free(session->si);
        session->si = NULL;
    }
    if (session->socket != KINETIC_SOCKET_DESCRIPTOR_INVALID) {
        KineticSocket_Close(session->socket);
        session->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }
    session->connected = false;
    return KINETIC_STATUS_CONNECTION_ERROR;
}

KineticStatus KineticSession_Disconnect(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    if (!session->connected || session->socket < 0) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }
    
    // Close the connection
    bus_release_socket(session->messageBus, session->socket, NULL);
    free(session->si);
    session->si = NULL;
    session->socket = KINETIC_HANDLE_INVALID;
    session->connected = false;
    pthread_mutex_destroy(&session->sendMutex);

    return KINETIC_STATUS_SUCCESS;
}

#define ATOMIC_FETCH_AND_INCREMENT(P) __sync_fetch_and_add(P, 1)

int64_t KineticSession_GetNextSequenceCount(KineticSession * const session)
{
    assert(session);
    int64_t seq_cnt = ATOMIC_FETCH_AND_INCREMENT(&session->sequence);
    return seq_cnt;
}

int64_t KineticSession_GetClusterVersion(KineticSession const * const session)
{
    assert(session);
    return session->config.clusterVersion;
}

void KineticSession_SetClusterVersion(KineticSession * const session, int64_t cluster_version)
{
    assert(session);
    session->config.clusterVersion = cluster_version;
}
