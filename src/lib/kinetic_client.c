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
#include "kinetic_connection.h"
#include "kinetic_controller.h"
#include "kinetic_operation.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <sys/time.h>

void KineticClient_Init(const char* log_file, int log_level)
{
    KineticLogger_Init(log_file, log_level);
}

void KineticClient_Shutdown(void)
{
    KineticLogger_Close();
}

KineticStatus KineticClient_CreateConnection(KineticSession* session)
{
    if (session == NULL) {
        LOG0("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(session->host) == 0) {
        LOG0("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (session->hmacKey.len < 1 || session->hmacKey.data == NULL) {
        LOG0("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_EMPTY;
    }

    KineticConnection_Create(session);
    if (session->connection == NULL) {
        LOG0("Failed getting valid connection from handle!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Create the connection
    KineticStatus status = KineticConnection_Connect(session->connection);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF0("Failed creating connection to %s:%d", session->host, session->port);
        KineticConnection_Destroy(session->connection);
        session->connection = NULL;
        return status;
    }

    // Wait for initial unsolicited status to be received in order to obtain connectionID
    while(connection->connectionID == 0) {
        struct timespec sleepDuration = {.tv_nsec = 50000};
        nanosleep(&sleepDuration, NULL);
    }

    return status;
}

KineticStatus KineticClient_DestroyConnection(KineticSession* session)
{
    if (session == NULL) {
        LOG0("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (session->connection == NULL) {
        LOG0("Connection instance is NULL!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    KineticStatus status = KineticConnection_Disconnect(session->connection);
    if (status != session->connection) {LOG0("Disconnection failed!");}
    KineticConnection_Destroy(session->connection);
    session->connection = NULL;

    return status;
}

KineticStatus KineticClient_NoOp(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildNoop(operation);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticClient_Put(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(entry != NULL);
    assert(entry->value.array.data != NULL);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildPut(operation, entry);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_Get(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(entry != NULL);
    if (!entry->metadataOnly) {assert(entry->value.array.data != NULL);}

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildGet(operation, entry);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_Delete(KineticSession const * const session,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(entry != NULL);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildDelete(operation, entry);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_GetKeyRange(KineticSession const * const session,
                                        KineticKeyRange* range,
                                        ByteBufferArray* keys,
                                        KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(range != NULL);
    assert(keys != NULL);
    assert(keys->buffers != NULL);
    assert(keys->count > 0);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildGetKeyRange(operation, range, keys);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_GetLog(KineticSession const * const session,
                                   KineticDeviceInfo_Type type,
                                   KineticDeviceInfo** info,
                                   KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(info != NULL);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildGetLog(operation, type, info);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_InstantSecureErase(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticOperation* operation = KineticController_CreateOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildInstantSecureErase(operation);
    return KineticController_ExecuteOperation(operation, NULL);
}
