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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include <string.h>


static KineticConnection ConnectionInstances[KINETIC_SESSIONS_MAX];
static KineticConnection* Connections[KINETIC_SESSIONS_MAX];


KineticConnection* KineticConnection_NewConnection(KineticSession* session)
{
    assert(session);
    session->handle = KINETIC_SESSION_INVALID;
    for (int handle = 1; handle <= KINETIC_SESSIONS_MAX; handle++)
    {
        if (Connections[handle-1] == NULL)
        {
            Connections[handle-1] = &ConnectionInstances[handle-1];
            session->handle = handle;
            *Connections[handle-1] = (KineticConnection){.session = session};
            return Connections[handle-1];
        }
    }
    return NULL;
}

void KineticConnection_FreeConnection(KineticSession* session)
{
    assert(session);
    assert(session->handle > KINETIC_SESSION_INVALID);
    assert(session->handle <= KINETIC_SESSIONS_MAX);
    if (Connections[session->handle-1] != NULL)
    {
        *Connections[handle-1] = {.session = KINETIC_SESSION_INVALID};
        Connections[handle-1] = NULL;
    }
    session->handle = KINETIC_SESSION_INVALID;
}


bool KineticConnection_Connect(KineticConnection* const connection)
{
    connection->connected = false;
    connection->nonBlocking = nonBlocking;
    connection->port = port;
    connection->socketDescriptor = -1;
    connection->clusterVersion = clusterVersion;
    connection->identity = identity;

    strcpy(connection->host, host);
    connection->key.data = connection->keyData;
    memcpy(connection->key.data, key.data, key.len);
    connection->key.len = key.len;

    connection->socketDescriptor = KineticSocket_Connect(
        connection->host, connection->port, nonBlocking);
    connection->connected = (connection->socketDescriptor >= 0);

    return connection->connected;
}

void KineticConnection_Disconnect(KineticConnection* connection)
{
    if (connection != NULL || connection->socketDescriptor >= 0)
    {
        close(connection->socketDescriptor);
        connection->socketDescriptor = -1;
    }
}

void KineticConnection_IncrementSequence(KineticConnection* const connection)
{
    connection->sequence++;
}
