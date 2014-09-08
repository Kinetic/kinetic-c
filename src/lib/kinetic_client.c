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

#include "kinetic_client.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_connection.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include <stdio.h>

KineticProto_Status_StatusCode KineticClient_ExecuteOperation(KineticOperation* operation);

void KineticClient_Init(const char* logFile)
{
    KineticLogger_Init(logFile);
}

bool KineticClient_Connect(
    KineticConnection* connection,
    const char* host,
    int port,
    bool nonBlocking,
    int64_t clusterVersion,
    int64_t identity,
    ByteArray key)
{
    if (connection == NULL)
    {
        LOG("Specified KineticConnection is NULL!");
        return false;
    }

    if (host == NULL)
    {
        LOG("Specified host is NULL!");
        return false;
    }

    if (key.len < 1)
    {
        LOG("Specified HMAC key is empty!");
        return false;
    }

    if (key.data == NULL)
    {
        LOG("Specified HMAC key is NULL!");
        return false;
    }

    // KINETIC_CONNECTION_INIT(connection, identity, key);

    if (!KineticConnection_Connect(connection, host, port, nonBlocking,
        clusterVersion, identity, key))
    {
        connection->connected = false;
        connection->socketDescriptor = -1;
        char message[64];
        sprintf(message, "Failed creating connection to %s:%d", host, port);
        LOG(message);
        return false;
    }

    connection->connected = true;

    return true;
}

void KineticClient_Disconnect(
    KineticConnection* connection)
{
   KineticConnection_Disconnect(connection);
}

KineticOperation KineticClient_CreateOperation(
    KineticConnection* connection,
    KineticPDU* request,
    KineticPDU* response)
{
    KineticOperation op;

    if (connection == NULL)
    {
        LOG("Specified KineticConnection is NULL!");
        assert(connection != NULL);
    }

    if (request == NULL)
    {
        LOG("Specified KineticPDU request is NULL!");
        assert(request != NULL);
    }

    if (response == NULL)
    {
        LOG("Specified KineticPDU response is NULL!");
        assert(response != NULL);
    }

    KineticPDU_Init(request, connection);
    KINETIC_PDU_INIT_WITH_MESSAGE(request, connection);
    KineticPDU_Init(response, connection);

    op.connection = connection;
    op.request = request;
    op.request->proto = &op.request->protoData.message.proto;
    op.response = response;

    return op;
}

KineticProto_Status_StatusCode KineticClient_NoOp(KineticOperation* operation)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);

    // Initialize request
    KineticOperation_BuildNoop(operation);

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticProto_Status_StatusCode KineticClient_Put(KineticOperation* operation,
    const Kinetic_KeyValue* metadata,
    const ByteArray value)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);
    assert(value.data != NULL);
    assert(value.len <= PDU_VALUE_MAX_LEN);

    // Initialize request
    KineticOperation_BuildPut(operation, metadata, value);

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticProto_Status_StatusCode KineticClient_Get(KineticOperation* operation,
    const Kinetic_KeyValue* metadata,
    const ByteArray value)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);
    assert(metadata != NULL);
    assert(metadata->key.data != NULL);
    assert(metadata->key.len <= KINETIC_MAX_KEY_LEN);

    ByteArray responseValue = BYTE_ARRAY_NONE;
    if (!metadata->metadataOnly)
    {
        if (value.data != NULL)
        {
            responseValue = value;
        }
        else
        {
            responseValue = (ByteArray){.data = operation->response->valueBuffer};
        }
    }

    // Initialize request
    KineticOperation_BuildGet(operation, metadata, responseValue);

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticProto_Status_StatusCode KineticClient_ExecuteOperation(KineticOperation* operation)
{
    KineticProto_Status_StatusCode status =
        KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;

    // Send the request
    if (KineticPDU_Send(operation->request))
    {
        // Associate response with same exchange as request
        operation->response->connection = operation->request->connection;

        // Receive the response
        if (KineticPDU_Receive(operation->response))
        {
            status = KineticPDU_Status(operation->response);
        }
    }

    return status;
}

