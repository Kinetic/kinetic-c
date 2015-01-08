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

#include "kinetic_session.h"
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

KineticStatus KineticSession_Create(KineticSession * const session, KineticClient * const client)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (client == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    session->connection = KineticAllocator_NewConnection();
    if (session->connection == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    KineticConnection_Init(session->connection);
    session->connection->session = session;
    session->connection->messageBus = client->bus;

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
    KineticAllocator_FreeConnection(session->connection);
    session->connection = NULL;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Connect(KineticSession * const session)
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


    connection->si = calloc(1, sizeof(socket_info) + 2 * PDU_PROTO_MAX_LEN);
    if (connection->si == NULL) { return KINETIC_STATUS_MEMORY_ERROR; }
    bool success = bus_register_socket(connection->messageBus, BUS_SOCKET_PLAIN, connection->socket, connection);
    if (!success) {
        free(connection->si);
        return KINETIC_STATUS_SESSION_INVALID;
    }

    connection->session = session;
    // #TODO what to do if we time out here? I think the bus should timeout by itself or something

    // Wait for initial unsolicited status to be received in order to obtain connectionID
    const long maxWaitMicrosecs = 2000000;
    long microsecsWaiting = 0;
    struct timespec sleepDuration = {.tv_nsec = 500000};
    while(connection->connectionID == 0) {
        if (microsecsWaiting > maxWaitMicrosecs) {
            LOG0("Timed out waiting for connection ID from device!");
            return KINETIC_STATUS_SOCKET_TIMEOUT;
        }
        nanosleep(&sleepDuration, NULL);
        microsecsWaiting += (sleepDuration.tv_nsec / 1000);
    }

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
    
    // Close the connection
    bus_release_socket(connection->messageBus, connection->socket);
    free(connection->si);

    connection->socket = KINETIC_HANDLE_INVALID;
    connection->connected = false;

    return KINETIC_STATUS_SUCCESS;
}

void KineticSession_IncrementSequence(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    session->connection->sequence++;
}
