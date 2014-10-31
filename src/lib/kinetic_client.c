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

#include "kinetic_client.h"
#include "kinetic_types_internal.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_connection.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <sys/time.h>

static KineticStatus KineticClient_CreateOperation(
    KineticOperation** operation,
    KineticSessionHandle handle)
{
    if (handle == KINETIC_HANDLE_INVALID) {
        LOG0("Specified session has invalid handle value");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    KineticConnection* connection = KineticConnection_FromHandle(handle);
    if (connection == NULL) {
        LOG0("Specified session is not associated with a connection");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    LOGF1("\n"
         "--------------------------------------------------\n"
         "Building new operation on connection @ 0x%llX", connection);

    *operation = KineticAllocator_NewOperation(connection);
    if (*operation == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    if ((*operation)->request == NULL) {
        return KINETIC_STATUS_NO_PDUS_AVAVILABLE;
    }

    return KINETIC_STATUS_SUCCESS;
}

static KineticStatus KineticClient_ExecuteOperation(KineticOperation* operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    LOGF1("Executing operation: 0x%llX", operation);
    if (operation->entry.value.array.data != NULL
        && operation->entry.value.bytesUsed > 0)
    {
        LOGF1("  Sending PDU (0x%0llX) w/value (%zu bytes)",
            operation->request, operation->entry.value.bytesUsed);
    }
    else {
        LOGF1("  Sending PDU (0x%0llX) w/o value", operation->request);
    }

    // Send the request
    status = KineticOperation_SendRequest(operation);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    return KineticOperation_ReceiveAsync(operation);
}

void KineticClient_Init(const char* log_file, int log_level)
{
    KineticLogger_Init(log_file, log_level);
}

void KineticClient_Shutdown(void)
{
    KineticLogger_Close();
}

KineticStatus KineticClient_Connect(const KineticSession* config,
                                    KineticSessionHandle* handle)
{
    if (handle == NULL) {
        LOG0("Session handle is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    *handle = KINETIC_HANDLE_INVALID;

    if (config == NULL) {
        LOG0("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(config->host) == 0) {
        LOG0("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (config->hmacKey.len < 1 || config->hmacKey.data == NULL) {
        LOG0("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_EMPTY;
    }

    // Obtain a new connection/handle
    *handle = KineticConnection_NewConnection(config);
    if (*handle == KINETIC_HANDLE_INVALID) {
        LOG0("Failed connecting to device!");
        return KINETIC_STATUS_SESSION_INVALID;
    }
    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (connection == NULL) {
        LOG0("Failed getting valid connection from handle!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Create the connection
    KineticStatus status = KineticConnection_Connect(connection);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF0("Failed creating connection to %s:%d", config->host, config->port);
        KineticConnection_FreeConnection(handle);
        *handle = KINETIC_HANDLE_INVALID;
        return status;
    }

    // Wait for initial unsolicited status to be received in order to obtain connectionID
    while(connection->connectionID == 0) {sleep(1);}

    return status;
}

KineticStatus KineticClient_Disconnect(KineticSessionHandle* const handle)
{
    if (*handle == KINETIC_HANDLE_INVALID) {
        LOG0("Invalid KineticSessionHandle specified!");
        return KINETIC_STATUS_SESSION_INVALID;
    }
    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (connection == NULL) {
        LOG0("Failed getting valid connection from handle!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Disconnect
    KineticStatus status = KineticConnection_Disconnect(connection);
    if (status != KINETIC_STATUS_SUCCESS) {LOG0("Disconnection failed!");}
    KineticConnection_FreeConnection(handle);
    *handle = KINETIC_HANDLE_INVALID;

    return status;
}

KineticStatus KineticClient_NoOp(KineticSessionHandle handle)
{
    KineticStatus status;
    KineticOperation* operation;
    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Initialize request
    KineticOperation_BuildNoop(operation);

    // Execute the operation
    status = KineticClient_ExecuteOperation(operation);

    return status;
}

KineticStatus KineticClient_Put(KineticSessionHandle handle,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    assert(entry != NULL);
    assert(entry->value.array.data != NULL);
    KineticStatus status;
    KineticOperation* operation;
    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Initialize request
    KineticOperation_BuildPut(operation, entry);
    if (closure != NULL) {operation->closure = *closure;}

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticStatus KineticClient_Get(KineticSessionHandle handle,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    assert(entry != NULL);
    if (!entry->metadataOnly) {assert(entry->value.array.data != NULL);}
    KineticStatus status;
    KineticOperation* operation;
    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Initialize request
    KineticOperation_BuildGet(operation, entry);
    if (closure != NULL) {operation->closure = *closure;}

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticStatus KineticClient_Delete(KineticSessionHandle handle,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure)
{
    assert(entry != NULL);
    KineticStatus status;
    KineticOperation* operation;
    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Initialize request
    KineticOperation_BuildDelete(operation, entry);
    if (closure != NULL) {operation->closure = *closure;}

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}

KineticStatus KineticClient_GetKeyRange(KineticSessionHandle handle,
                                        KineticKeyRange* range,
                                        ByteBufferArray* keys,
                                        KineticCompletionClosure* closure)
{
    assert(handle != KINETIC_HANDLE_INVALID);
    assert(range != NULL);
    assert(keys != NULL);
    assert(keys->buffers != NULL);
    assert(keys->count > 0);

    KineticStatus status;
    KineticOperation* operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildGetKeyRange(operation, range, keys);
    if (closure != NULL) {operation->closure = *closure;}

    // Execute the operation
    return KineticClient_ExecuteOperation(operation);
}
