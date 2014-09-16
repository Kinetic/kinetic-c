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
#include <stdlib.h>

static KineticConnection ConnectionInstances[KINETIC_SESSIONS_MAX];
static KineticConnection* Connections[KINETIC_SESSIONS_MAX];
static int PDUsPerSession = KINETIC_PDUS_PER_SESSION_DEFAULT;

KineticConnection* KineticConnection_NewConnection(KineticSession* session)
{
    if (session == NULL)
    {
        return NULL;
    }
    session->handle = SESSION_HANDLE_INVALID;
    for (int handle = 1; handle <= KINETIC_SESSIONS_MAX; handle++)
    {
        int idx = session->handle - 1;
        if (Connections[idx] == NULL)
        {
            Connections[idx] = &ConnectionInstances[idx];
            session->handle = handle;
            *Connections[idx] = (KineticConnection){.session = session};
            for (int i = 0; i < PDUsPerSession; i++)
            {
                Connections[idx]->pdus[i] = malloc(sizeof(KineticPDU));
            }
            return Connections[idx];
        }
    }
    return NULL;
}

void KineticConnection_FreeConnection(KineticSession* session)
{
    assert(session);
    assert(session->handle > SESSION_HANDLE_INVALID);
    assert(session->handle <= KINETIC_SESSIONS_MAX);
    int idx = session->handle - 1;
    if (Connections[idx] != NULL)
    {
        *Connections[idx] =
            (KineticConnection) {.session = SESSION_HANDLE_INVALID};
        for (int i = 0; i < KINETIC_PDUS_PER_SESSION_MAX; i++)
        {
            if (Connections[idx]->pdus[i] != NULL)
            {
                free(Connections[idx]->pdus[i]);
                Connections[idx]->pdus[i] = NULL;
            }
        }
        Connections[idx] = NULL;
    }
    session->handle = SESSION_HANDLE_INVALID;
}

KineticConnection* KineticConnection_GetFromSession(KineticSession* session)
{
    assert(session);
    assert(session->handle > SESSION_HANDLE_INVALID);
    assert(session->handle <= KINETIC_SESSIONS_MAX);
    return Connections[session->handle];
}

bool KineticConnection_Connect(KineticConnection* const connection)
{
    assert(connection != NULL);
    assert(connection->session != NULL);
    connection->connected = false;
    connection->socket = -1;

    connection->socket = KineticSocket_Connect(
        connection->session->host,
        connection->session->port,
        connection->session->nonBlocking);
    connection->connected = (connection->socket >= 0);

    return connection->connected;
}

void KineticConnection_Disconnect(KineticConnection* connection)
{
    if (connection != NULL || connection->socket >= 0)
    {
        close(connection->socket);
        connection->socket = -1;
    }
}

void KineticConnection_IncrementSequence(KineticConnection* const connection)
{
    connection->sequence++;
}
