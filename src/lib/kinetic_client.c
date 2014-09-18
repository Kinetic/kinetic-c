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
#include "kinetic_types_internal.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_connection.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include <stdlib.h>

static KineticStatus KineticClient_ExecuteOperation(KineticOperation* operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    // Send the request
    if (KineticPDU_Send(operation->request))
    {
        // Associate response with same exchange as request
        operation->response->connection = operation->request->connection;

        // Receive the response
        if (KineticPDU_Receive(operation->response))
        {
            status = KineticOperation_GetStatus(operation);
        }
    }

    return status;
}

KineticStatus KineticClient_Connect(const KineticSession* config,
    KineticSessionHandle* const handle)
{
    if (config == NULL)
    {
        LOG("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(config->host) == 0)
    {
        LOG("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (config->hmacKey.len < 1 || config->hmacKey.data == NULL)
    {
        LOG("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_EMPTY;
    }

    *handle = KineticConnection_NewConnection(config);
    if (handle == KINETIC_HANDLE_INVALID)
    {
        LOG("Failed connecting to device!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (!KineticConnection_Connect(connection))
    {
        LOGF("Failed creating connection to %s:%d",
            config->host, config->port);
        KineticConnection_FreeConnection(handle);
        *handle = KINETIC_HANDLE_INVALID;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticClient_Disconnect(KineticSessionHandle* const handle)
{
    if (*handle == KINETIC_HANDLE_INVALID)
    {
        LOG("Invalid KineticSessionHandle specified!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (connection == NULL)
    {
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticConnection_Disconnect(connection);
    KineticConnection_FreeConnection(handle);
    
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticClient_NoOp(KineticSessionHandle session)
{
    if (session == KINETIC_HANDLE_INVALID)
    {
        LOG("Specified session has invalid handle value");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    KineticConnection* connection = KineticConnection_FromHandle(session);
    if (connection == NULL)
    {
        LOG("Specified session is not associated with a connection");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticOperation operation;
    KineticStatus status = KineticOperation_Create(&operation, connection);
    if (status != KINETIC_STATUS_SUCCESS)
    {
        return status;
    }

    // Initialize request
    KineticOperation_BuildNoop(&operation);

    // Execute the operation
    return KineticClient_ExecuteOperation(&operation);
}

#if 0
KineticStatus KineticClient_Put(KineticSessionHandle session,
    const KineticKeyValue* const metadata)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);
    assert(metadata != NULL);
    assert(metadata->value.data != NULL);
    assert(metadata->value.len <= PDU_VALUE_MAX_LEN);

    // Initialize request
    KineticOperation_BuildPut(operation, metadata);

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticStatus KineticClient_Get(KineticSessionHandle session,
    const KineticKeyValue* metadata)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);
    assert(metadata != NULL);
    assert(metadata->key.data != NULL);
    assert(metadata->key.len <= KINETIC_MAX_KEY_LEN);

    if (!metadata->metadataOnly)
    {
        if (metadata->value.data == NULL)
        {
             metadata->value = (ByteArray){
                .data = operation->response->valueBuffer,
                .len = PDU_VALUE_MAX_LEN};
        }
    }

    // Initialize request
    KineticOperation_BuildGet(operation, metadata);

    // Execute the operation
    KineticStatus status = KineticClient_ExecuteOperation(operation);

    // Update the metadata with the received value length upon success
    if (status == KINETIC_STATUS_SUCCESS)
    {
        metadata->value.len = operation->response->value.len;
    }
    else
    {
        metadata->value.len = 0;
    }

    return status;
}

KineticStatus KineticClient_Delete(KineticSessionHandle session,
    const KineticKeyValue* const metadata)
{
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);
    assert(metadata != NULL);
    assert(metadata->key.data != NULL);
    assert(metadata->key.len > 0);

    // Initialize request
    KineticOperation_BuildDelete(operation, metadata);

    // Execute the operation
    KineticStatus status = KineticClient_ExecuteOperation(operation);

    // Zero out value length for all DELETE operations
    operation->response->value.len = 0;
    metadata->value.len = 0;

    return status;
}
#endif
