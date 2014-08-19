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

#include "put.h"

int Put(
    const char* host,
    int port,
    int64_t clusterVersion,
    int64_t identity,
    const char* hmacKey,
    const uint8_t* value,
    int64_t valueLength,
    const char* valueKey,
    const char* version,
    const char* newVersion)
{
    // KineticOperation operation;
    // KineticPDU request, response;
    // KineticConnection connection;
    // KineticMessage requestMsg;
    KineticProto_Status_StatusCode status = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
    // bool success;
    // uint8_t value[PDU_VALUE_MAX_LEN];

    // KineticClient_Init(NULL);
    // success = KineticClient_Connect(&connection, host, port, false, clusterVersion);
    // assert(success);
    // success = KineticClient_ConfigureExchange(&exchange, &connection, clusterVersion, identity, key, strlen(key));
    // assert(success);
    // operation = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);
    // status = KineticClient_Put(&operation, value, sizeof(value));

    // if (status == KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS)
    // {
    //     printf("Put operation completed successfully. Your data has been stored!\n");
    // }

    // KineticClient_Disconnect(&connection);
    return status;
}
