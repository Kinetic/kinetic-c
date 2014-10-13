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

static KineticStatus KineticClient_CreateOperation(
    KineticOperation* const operation,
    KineticSessionHandle handle)
{
    if (handle == KINETIC_HANDLE_INVALID) {
        LOG("Specified session has invalid handle value");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    KineticConnection* connection = KineticConnection_FromHandle(handle);
    if (connection == NULL) {
        LOG("Specified session is not associated with a connection");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    *operation = KineticOperation_Create(connection);
    if (operation->request == NULL || operation->response == NULL) {
        return KINETIC_STATUS_NO_PDUS_AVAVILABLE;
    }

    return KINETIC_STATUS_SUCCESS;
}

static KineticStatus KineticClient_ExecuteOperation(KineticOperation* operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    LOGF("Executing operation: 0x%llX", operation);
    if (operation->request->entry.value.array.data != NULL
        && operation->request->entry.value.bytesUsed > 0) {
        LOGF("  Sending PDU w/value (%zu bytes)",
             operation->request->entry.value.bytesUsed);
    }
    else {
        LOG("  Sending PDU w/o value");
    }

    // Send the request
    status = KineticPDU_Send(operation->request);
    if (status == KINETIC_STATUS_SUCCESS) {
        // Associate response with same exchange as request
        operation->response->connection = operation->request->connection;

        // Receive the response
        status = KineticPDU_Receive(operation->response);
        if (status == KINETIC_STATUS_SUCCESS) {
            status = KineticOperation_GetStatus(operation);
        }
    }

    return status;
}

void KineticClient_Init(const char* logFile)
{
    KineticLogger_Init(logFile);
}

void KineticClient_Shutdown(void)
{
    KineticLogger_Close();
}

KineticStatus KineticClient_Connect(const KineticSession* config,
                                    KineticSessionHandle* handle)
{
    if (handle == NULL) {
        LOG("Session handle is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    *handle = KINETIC_HANDLE_INVALID;

    if (config == NULL) {
        LOG("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(config->host) == 0) {
        LOG("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (config->hmacKey.len < 1 || config->hmacKey.data == NULL) {
        LOG("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_EMPTY;
    }

    // Obtain a new connection/handle
    *handle = KineticConnection_NewConnection(config);
    if (handle == KINETIC_HANDLE_INVALID) {
        LOG("Failed connecting to device!");
        return KINETIC_STATUS_SESSION_INVALID;
    }
    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (connection == NULL) {
        LOG("Failed getting valid connection from handle!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Create the connection
    KineticStatus status = KineticConnection_Connect(connection);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF("Failed creating connection to %s:%d", config->host, config->port);
        KineticConnection_FreeConnection(handle);
        *handle = KINETIC_HANDLE_INVALID;
        return status;
    }

    // Retrieve initial connection status info
    status = KineticConnection_ReceiveDeviceStatusMessage(connection);

    return status;
}

KineticStatus KineticClient_Disconnect(KineticSessionHandle* const handle)
{
    if (*handle == KINETIC_HANDLE_INVALID) {
        LOG("Invalid KineticSessionHandle specified!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticConnection* connection = KineticConnection_FromHandle(*handle);
    if (connection == NULL) {
        LOG("Failed getting valid connection from handle!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    KineticStatus status = KineticConnection_Disconnect(connection);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG("Disconnection failed!");
    }

    KineticConnection_FreeConnection(handle);
    *handle = KINETIC_HANDLE_INVALID;

    return status;
}

KineticStatus KineticClient_NoOp(KineticSessionHandle handle)
{
    KineticStatus status;
    KineticOperation operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildNoop(&operation);

    // Execute the operation
    status = KineticClient_ExecuteOperation(&operation);

    KineticOperation_Free(&operation);

    return status;
}

KineticStatus KineticClient_Put(KineticSessionHandle handle,
                                KineticEntry* const entry)
{
    KineticStatus status;
    KineticOperation operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildPut(&operation, entry);

    // Execute the operation
    status = KineticClient_ExecuteOperation(&operation);

    if (status == KINETIC_STATUS_SUCCESS) {
        // Propagate newVersion to dbVersion in metadata, if newVersion specified
        if (entry->newVersion.array.data != NULL && entry->newVersion.array.len > 0) {
            entry->dbVersion = entry->newVersion;
            entry->newVersion = BYTE_BUFFER_NONE;
        }
    }

    KineticOperation_Free(&operation);

    return status;
}

KineticStatus KineticClient_Get(KineticSessionHandle handle,
                                KineticEntry* const entry)
{
    assert(entry != NULL);
    if (!entry->metadataOnly) {
        assert(entry->value.array.data != NULL);
    }

    KineticStatus status;
    KineticOperation operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildGet(&operation, entry);

    // Execute the operation
    status = KineticClient_ExecuteOperation(&operation);

    // Update the entry upon success
    if (status == KINETIC_STATUS_SUCCESS) {
        entry->value.bytesUsed = operation.response->entry.value.bytesUsed;
        KineticProto_Command_KeyValue* keyValue = KineticPDU_GetKeyValue(operation.response);
        if (keyValue != NULL) {
            if (!Copy_KineticProto_Command_KeyValue_to_KineticEntry(keyValue, entry)) {
                status = KINETIC_STATUS_BUFFER_OVERRUN;
            }
        }
    }

    KineticOperation_Free(&operation);

    return status;
}

KineticStatus KineticClient_Delete(KineticSessionHandle handle,
                                   KineticEntry* const entry)
{
    KineticStatus status;
    KineticOperation operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildDelete(&operation, entry);

    // Execute the operation
    status = KineticClient_ExecuteOperation(&operation);

    KineticOperation_Free(&operation);

    return status;
}

// command {
//   header {
//     // See above for descriptions of these fields
//     clusterVersion: ...
//     identity: ...
//     connectionID: ...
//     sequence: ...
// 
//     // messageType should be GETKEYRANGE
//     messageType: GETKEYRANGE
//   }
//   body {
//     // The range message must be populated
//     range {
//       // Required bytes, the beginning of the requested range
//       startKey: "..."
// 
//       // Optional bool, defaults to false
//       // True indicates that the start key should be included in the returned
//       // range
//       startKeyInclusive: ...
// 
//       // Required bytes, the end of the requested range
//       endKey: "..."
// 
//       // Optional bool, defaults to false
//       // True indicates that the end key should be included in the returned
//       // range
//       endKeyInclusive: ...
// 
//       // Required int32, must be greater than 0
//       // The maximum number of keys returned, in sorted order
//       maxReturned: ...
// 
//       // Optional bool, defaults to false
//       // If true, the key range will be returned in reverse order, starting at
//       // endKey and moving back to startKey.  For instance
//       // if the search is startKey="j", endKey="k", maxReturned=2,
//       // reverse=true and the keys "k0", "k1", "k2" exist
//       // the system will return "k2" and "k1" in that order.
//       reverse: ....
//     }
//   }
// }
KineticStatus KineticClient_GetKeyRange(KineticSessionHandle handle,
                                        KineticKeyRange* range, ByteBuffer keys[], int max_keys)
{
    assert(handle != KINETIC_HANDLE_INVALID);
    assert(range != NULL);
    assert(keys != NULL);
    assert(max_keys > 0);

    KineticStatus status;
    KineticOperation operation;

    status = KineticClient_CreateOperation(&operation, handle);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }

    // Initialize request
    KineticOperation_BuildGetKeyRange(&operation, range);

    // Execute the operation
    status = KineticClient_ExecuteOperation(&operation);

    // Report the key list upon success
    if (status == KINETIC_STATUS_SUCCESS) {
        KineticProto_Command_Range* keyRange = KineticPDU_GetKeyRange(operation.response);
        if (keyRange != NULL) {
            if (!Copy_KineticProto_Command_Range_to_buffer_list(keyRange, keys, max_keys)) {
                status = KINETIC_STATUS_BUFFER_OVERRUN;
            }
        }
    }

    KineticOperation_Free(&operation);

    return status;
}
