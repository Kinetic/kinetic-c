/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "kinetic_types_internal.h"
#include "kinetic_version_info.h"
#include "kinetic_client.h"
#include "kinetic_allocator.h"
#include "kinetic_session.h"
#include "kinetic_controller.h"
#include "kinetic_operation.h"
#include "kinetic_builder.h"
#include "kinetic_logger.h"
#include "kinetic_response.h"
#include "kinetic_bus.h"
#include "kinetic_memory.h"
#include <stdlib.h>
#include <sys/time.h>

static const KineticVersionInfo VersionInfo = {
    .version = KINETIC_C_VERSION,
    .protocolVersion = KINETIC_C_PROTOCOL_VERSION,
    .repoCommitHash = KINETIC_C_REPO_HASH,
};

const KineticVersionInfo * KineticClient_Version(void)
{
    return &VersionInfo;
}

KineticClient * KineticClient_Init(KineticClientConfig *config)
{
    KineticLogger_Init(config->logFile, config->logLevel);
    KineticClient * client = KineticCalloc(1, sizeof(*client));
    if (client == NULL) {
        KineticLogger_Close();
        return NULL;
    }

    /* Use defaults if set to 0. */
    if (config->readerThreads == 0) {
        config->readerThreads = KINETIC_CLIENT_DEFAULT_READER_THREADS;
    }
    if (config->maxThreadpoolThreads == 0) {
        config->maxThreadpoolThreads = KINETIC_CLIENT_DEFAULT_MAX_THREADPOOL_THREADS;
    }

    bool success = KineticBus_Init(client, config);
    if (!success) {
        KineticFree(client);
        KineticLogger_Close();
        return NULL;
    }
    return client;
}

void KineticClient_Shutdown(KineticClient * const client)
{
    KineticBus_Shutdown(client);
    KineticFree(client);
    KineticLogger_Close();
}

KineticStatus KineticClient_CreateSession(KineticSessionConfig* const config,
    KineticClient * const client, KineticSession** session)
{
    if (config == NULL) {
        LOG0("KineticSessionConfig is NULL!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (session == NULL) {
        LOG0("Pointer to KineticSession pointer is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(config->host) == 0) {
        LOG0("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (config->hmacKey.len < 1 || config->hmacKey.data == NULL)
    {
        LOG0("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_REQUIRED;
    }

    // Create a new session
    KineticSession* s = KineticAllocator_NewSession(client->bus, config);
    if (s == NULL) {
        LOG0("Failed to create session instance!");
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    KineticStatus status = KineticSession_Create(s, client);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to create session instance!");
        KineticAllocator_FreeSession(s);
        return status;
    }

    // Establish the connection
    status = KineticSession_Connect(s);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF0("Failed creating connection to %s:%d", config->host, config->port);
        KineticAllocator_FreeSession(s);
        return status;
    }

    *session = s;

    return status;
}

KineticStatus KineticClient_DestroySession(KineticSession* const session)
{
    if (session == NULL) {
        LOG0("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    KineticStatus status = KineticSession_Disconnect(session);
    if (status != KINETIC_STATUS_SUCCESS) {LOG0("Disconnection failed!");}
    KineticSession_Destroy(session);

    return status;
}

KineticStatus KineticClient_GetTerminationStatus(KineticSession * const session)
{
    return KineticSession_GetTerminationStatus(session);
}

KineticStatus KineticClient_NoOp(KineticSession* const session)
{
    KINETIC_ASSERT(session);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildNoop(operation);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticClient_Put(KineticSession* const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);
    KINETIC_ASSERT(entry);

    // Assert non-NULL value upon non-zero length
    if (entry->value.array.len > 0) {
        KINETIC_ASSERT(entry->value.array.data);
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}
    KINETIC_ASSERT(operation->session == session);

    // Initialize request
    KineticStatus status = KineticBuilder_BuildPut(operation, entry);
    if (status != KINETIC_STATUS_SUCCESS) {
        KineticAllocator_FreeOperation(operation);
        return status;
    }

    // Execute the operation
    KineticStatus res = KineticController_ExecuteOperation(operation, closure);
    return res;
}

KineticStatus KineticClient_Flush(KineticSession* const session,
                                  KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) { return KINETIC_STATUS_MEMORY_ERROR; }

    // Initialize request
    KineticBuilder_BuildFlush(operation);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

static bool has_key(KineticEntry* const entry)
{
    return entry->key.array.data != NULL;
}

static bool has_value_buffer(KineticEntry* const entry)
{
    return entry->value.array.data != NULL;
}

typedef enum {
    CMD_GET,
    CMD_GET_NEXT,
    CMD_GET_PREVIOUS,
} GET_COMMAND;

static KineticStatus handle_get_command(GET_COMMAND cmd,
                                        KineticSession* const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);
    KINETIC_ASSERT(entry);

    if (!has_key(entry)) {return KINETIC_STATUS_MISSING_KEY;}
    if (!has_value_buffer(entry) && !entry->metadataOnly) {
        return KINETIC_STATUS_MISSING_VALUE_BUFFER;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    // Initialize request
    switch (cmd)
    {
    case CMD_GET:
        KineticBuilder_BuildGet(operation, entry);
        break;
    case CMD_GET_NEXT:
        KineticBuilder_BuildGetNext(operation, entry);
        break;
    case CMD_GET_PREVIOUS:
        KineticBuilder_BuildGetPrevious(operation, entry);
        break;
    default:
        KINETIC_ASSERT(false);
    }

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_Get(KineticSession* const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET, session, entry, closure);
}

KineticStatus KineticClient_GetPrevious(KineticSession* const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET_PREVIOUS, session, entry, closure);
}

KineticStatus KineticClient_GetNext(KineticSession* const session,
                                    KineticEntry* const entry,
                                    KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET_NEXT, session, entry, closure);
}

KineticStatus KineticClient_Delete(KineticSession* const session,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);
    KINETIC_ASSERT(entry);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticBuilder_BuildDelete(operation, entry);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_GetKeyRange(KineticSession* const session,
                                        KineticKeyRange* range,
                                        ByteBufferArray* keys,
                                        KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);
    KINETIC_ASSERT(range);
    KINETIC_ASSERT(keys);
    KINETIC_ASSERT(keys->buffers);
    KINETIC_ASSERT(keys->count > 0);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticBuilder_BuildGetKeyRange(operation, range, keys);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_P2POperation(KineticSession* const session,
                                         KineticP2P_Operation* const p2pOp,
                                         KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session);
    KINETIC_ASSERT(p2pOp);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticStatus status = KineticBuilder_BuildP2POperation(operation, p2pOp);
    if (status != KINETIC_STATUS_SUCCESS) {
        // TODO we need to find a more generic way to handle errors on command construction
        if (closure != NULL) {
            operation->closure = *closure;
        }
        return status;
    }

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}
