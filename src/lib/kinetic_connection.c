/*
 * kinetic-c-client
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

KineticConnection KineticConnection_Create(void)
{
    KineticConnection connection;
    memset(&connection, 0, sizeof(connection));
    connection.Blocking = true;
    connection.FileDescriptor = -1;
    return connection;
}

bool KineticConnection_Connect(
    KineticConnection* const connection,
    const char* host, int port, bool blocking)
{
    connection->Connected = false;
    connection->Blocking = blocking;
    connection->Port = port;
    connection->FileDescriptor = -1;
    strcpy(connection->Host, host);

    connection->FileDescriptor = KineticSocket_Connect(
        connection->Host, connection->Port, blocking);
    connection->Connected = (connection->FileDescriptor >= 0);

    return connection->Connected;
}

bool KineticConnection_SendMessage(
    KineticConnection* const connection,
    KineticMessage* const message)
{
    return false;
}

bool KineticConnection_ReceiveMessage(
    KineticConnection* const connection,
    KineticMessage* const reponse)
{
    return KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
}
