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

#include "kinetic_api.h"
#include "kinetic_connection.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include <stdio.h>

void KineticApi_Init(const char* log_file)
{
    KineticLogger_Init(log_file);
}

const KineticConnection KineticApi_Connect(const char* host, int port, bool blocking)
{
    KineticConnection connection = KineticConnection_Init();

    if (!KineticConnection_Connect(&connection, host, port, blocking))
    {
        connection.Connected = false;
        connection.FileDescriptor = -1;
        char message[64];
        sprintf(message, "Failed creating connection to %s:%d", host, port);
        LOG(message);
    }
    else
    {
        connection.Connected = true;
    }

	return connection;
}

KineticProto_Status_StatusCode KineticApi_SendNoop(KineticPDU* request, KineticPDU* response)
{
    KineticProto_Status_StatusCode status =
        KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;

    // Initialize request
    KineticExchange_Init(request->exchange, 1234, 5678, request->exchange->connection);
    KineticMessage_Init(request->protobuf);
    KineticMessage_BuildNoop(request->protobuf);

    KineticPDU_Init(request, request->exchange, request->protobuf, NULL, 0);

    // Send the request
    KineticConnection_SendPDU(request);

    // Associate response with same exchange as request
    KineticPDU_Init(response, response->exchange, response->protobuf, NULL, 0);
    if (KineticConnection_ReceivePDU(response))
    {
        status = response->protobuf->command.status->code;
    }

	return status;
}
