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
#include "kinetic_controller.h"
#include "kinetic_socket.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_controller.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

STATIC KineticConnection ConnectionInstances[KINETIC_SESSIONS_MAX];
STATIC KineticConnection* Connections[KINETIC_SESSIONS_MAX];

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
    pthread_mutex_destroy(&connection->writeMutex);
    pthread_mutex_destroy(&connection->readMutex);
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
                             connection->session.port);
    connection->connected = (connection->socket >= 0);
    if (!connection->connected) {
        LOG0("Session connection failed!");
        connection->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    if (!connection->session.no_threads) {
        // Kick off the worker thread
        connection->thread.connection = connection;
        KineticController_CreateWorkerThreads(connection);
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

    if (!connection->session.no_threads) {
        connection->thread.abortRequested = true;
        LOG3("\nSent abort request to worker thread!\n");
        int pthreadStatus = pthread_join(connection->threadID, NULL);
        if (pthreadStatus != 0) {
            char errMsg[256];
            Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
            LOGF0("Failed terminating worker thread w/error: %s", errMsg);
            status = KINETIC_STATUS_CONNECTION_ERROR;
        }
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
