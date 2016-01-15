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

#include "kinetic_operation.h"
#include "kinetic_controller.h"
#include "kinetic_session.h"
#include "kinetic_message.h"
#include "kinetic_bus.h"
#include "kinetic_response.h"
#include "kinetic_device_info.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic_request.h"
#include "kinetic_acl.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "kinetic_acl.h"

/*******************************************************************************
 * Standard Client Operations
*******************************************************************************/

KineticStatus KineticCallbacks_Basic(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    return status;
}

KineticStatus KineticCallbacks_Put(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KINETIC_ASSERT(operation->entry != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
        // Propagate newVersion to dbVersion in metadata, if newVersion specified
        KineticEntry* entry = operation->entry;
        if (entry->newVersion.array.data != NULL && entry->newVersion.array.len > 0) {
            // If both buffers supplied, copy newVersion into dbVersion, and clear newVersion
            if (entry->dbVersion.array.data != NULL && entry->dbVersion.array.len > 0) {
                ByteBuffer_Reset(&entry->dbVersion);
                ByteBuffer_Append(&entry->dbVersion, entry->newVersion.array.data, entry->newVersion.bytesUsed);
                ByteBuffer_Reset(&entry->newVersion);
            }

            // If only newVersion buffer supplied, move newVersion buffer into dbVersion,
            // and set newVersion to NULL buffer
            else {
                entry->dbVersion = entry->newVersion;
                entry->newVersion = BYTE_BUFFER_NONE;
            }
        }
    }
    return status;
}

KineticStatus KineticCallbacks_Get(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KINETIC_ASSERT(operation->entry != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
        // Update the entry upon success
        Com__Seagate__Kinetic__Proto__Command__KeyValue* keyValue = KineticResponse_GetKeyValue(operation->response);
        if (keyValue != NULL) {
            if (!Copy_Com__Seagate__Kinetic__Proto__Command__KeyValue_to_KineticEntry(keyValue, operation->entry)) {
                return KINETIC_STATUS_BUFFER_OVERRUN;
            }
        }

        if (!operation->entry->metadataOnly &&
            !ByteBuffer_IsNull(operation->entry->value))
        {
            ByteBuffer_AppendArray(&operation->entry->value, (ByteArray){
                .data = operation->response->value,
                .len = operation->response->header.valueLength,
            });
        }
    }

    return status;
}

KineticStatus KineticCallbacks_Delete(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KINETIC_ASSERT(operation->entry != NULL);
    return status;
}

KineticStatus KineticCallbacks_GetKeyRange(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KINETIC_ASSERT(operation->buffers != NULL);
    KINETIC_ASSERT(operation->buffers->count > 0);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
        // Report the key list upon success
        Com__Seagate__Kinetic__Proto__Command__Range* keyRange = KineticResponse_GetKeyRange(operation->response);
        if (keyRange != NULL) {
            if (!Copy_Com__Seagate__Kinetic__Proto__Command__Range_to_ByteBufferArray(keyRange, operation->buffers)) {
                return KINETIC_STATUS_BUFFER_OVERRUN;
            }
        }
    }
    return status;
}

static void populateP2PStatusCodes(KineticP2P_Operation* const p2pOp, Com__Seagate__Kinetic__Proto__Command__P2POperation const * const p2pOperation)
{
    if (p2pOperation == NULL) { return; }
    for(size_t i = 0; i < p2pOp->numOperations; i++)
    {
        if (i < p2pOperation->n_operation)
        {
            if ((p2pOperation->operation[i]->status != NULL) &&
                (p2pOperation->operation[i]->status->has_code))
            {
                p2pOp->operations[i].resultStatus = KineticProtoStatusCode_to_KineticStatus(
                    p2pOperation->operation[i]->status->code);
            }
            else
            {
                p2pOp->operations[i].resultStatus = KINETIC_STATUS_INVALID;
            }
            if ((p2pOp->operations[i].chainedOperation != NULL) &&
                 (p2pOperation->operation[i]->p2pop != NULL)) {
                populateP2PStatusCodes(p2pOp->operations[i].chainedOperation, p2pOperation->operation[i]->p2pop);
            }
        }
        else
        {
            p2pOp->operations[i].resultStatus = KINETIC_STATUS_INVALID;
        }
    }
}

KineticStatus KineticCallbacks_P2POperation(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KineticP2P_Operation* const p2pOp = operation->p2pOp;

    if (status == KINETIC_STATUS_SUCCESS)
    {
        if ((operation->response != NULL) &&
            (operation->response->command != NULL) &&
            (operation->response->command->body != NULL) &&
            (operation->response->command->body->p2poperation != NULL)) {
            populateP2PStatusCodes(p2pOp, operation->response->command->body->p2poperation);
        }
    }

    KineticAllocator_FreeP2PProtobuf(operation->request->command->body->p2poperation);

    return status;
}


/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/

KineticStatus KineticCallbacks_GetLog(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    KINETIC_ASSERT(operation->deviceInfo != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
        // Copy the data from the response protobuf into a new info struct
        if (operation->response->command->body->getlog == NULL) {
            return KINETIC_STATUS_OPERATION_FAILED;
        }
        else {
            *operation->deviceInfo = KineticLogInfo_Create(operation->response->command->body->getlog);
            return KINETIC_STATUS_SUCCESS;
        }
    }
    return status;
}

KineticStatus KineticCallbacks_SetClusterVersion(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    if (status == KINETIC_STATUS_SUCCESS) {
        KineticSession_SetClusterVersion(operation->session, operation->pendingClusterVersion);
        operation->pendingClusterVersion = -1; // Invalidate
    }
    return status;
}

KineticStatus KineticCallbacks_SetACL(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);
    if (operation->request != NULL &&
        operation->request->command != NULL &&
        operation->request->command->body != NULL &&
        operation->request->command->body->security != NULL &&
        operation->request->command->body->security->acl != NULL)
    {
        struct ACL* acls = calloc(1, sizeof(struct ACL));
        acls->ACL_count = operation->request->command->body->security->n_acl,
        acls->ACLs = operation->request->command->body->security->acl,
        KineticACL_Free(acls);
    }
    return status;
}

KineticStatus KineticCallbacks_UpdateFirmware(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->session != NULL);

    if (operation->value.data != NULL) {
        free(operation->value.data);
        memset(&operation->value, 0, sizeof(ByteArray));
    }

    return status;
}
