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

KineticStatus KineticSession_Create(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    session->connection = malloc(sizeof(KineticConnection));
    if (session->connection == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    KINETIC_CONNECTION_INIT(session->connection);
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Destroy(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    if (session->connection == NULL) {
        return KINETIC_STATUS_SESSION_INVALID;
    }
    pthread_mutex_destroy(&session->connection->writeMutex);
    session->connection = NULL;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Connect(KineticSession const * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    KineticConnection* connection = session->connection;
    if (connection == NULL) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Establish the connection
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(strlen(session->config.host) > 0);
    connection->connected = false;
    connection->socket = KineticSocket_Connect(
        session->config.host, session->config.port);
    connection->connected = (connection->socket >= 0);
    if (!connection->connected) {
        LOG0("Session connection failed!");
        connection->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Kick off the worker thread
    session->connection->thread.connection = connection;
    KineticController_Init(session);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Disconnect(KineticSession const * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    KineticConnection* connection = session->connection;
    if (connection == NULL || !session->connection->connected || connection->socket < 0) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Shutdown the worker thread
    KineticStatus status = KINETIC_STATUS_SUCCESS;
    connection->thread.abortRequested = true;
    LOG3("\nSent abort request to worker thread!\n");
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

void KineticSession_IncrementSequence(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    session->connection->sequence++;
}
