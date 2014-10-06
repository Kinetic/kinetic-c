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

#include "kinetic_operation.h"
#include "kinetic_connection.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>

static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->proto->command != NULL);
    assert(operation->request->proto->command->header != NULL);
    assert(operation->response != NULL);
}

KineticOperation KineticOperation_Create(KineticConnection* const connection)
{
    LOGF("\n"
         "--------------------------------------------------\n"
         "Building new operation on connection @ 0x%llX", connection);

    KineticOperation operation = {
        .connection = connection,
        .request = KineticAllocator_NewPDU(&connection->pdus),
        .response =  KineticAllocator_NewPDU(&connection->pdus),
    };

    if (operation.request == NULL) {
        LOG("Request PDU could not be allocated!"
            " Try reusing or freeing a PDU.");
        return (KineticOperation) {
            .request = NULL, .response = NULL
        };
    }
    KineticPDU_Init(operation.request, connection);
    KINETIC_PDU_INIT_WITH_MESSAGE(operation.request, connection);
    operation.request->proto = &operation.request->protoData.message.proto;

    if (operation.response == NULL) {
        LOG("Response PDU could not be allocated!"
            " Try reusing or freeing a PDU.");
        return (KineticOperation) {
            .request = NULL, .response = NULL
        };
    }

    KineticPDU_Init(operation.response, connection);

    return operation;
}

KineticStatus KineticOperation_Free(KineticOperation* const operation)
{
    if (operation == NULL) {
        LOG("Specified operation is NULL so nothing to free");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (operation->request != NULL) {
        KineticAllocator_FreePDU(&operation->connection->pdus, operation->request);
        operation->request = NULL;
    }

    if (operation->response != NULL) {
        KineticAllocator_FreePDU(&operation->connection->pdus, operation->response);
        operation->response = NULL;
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticOperation_GetStatus(const KineticOperation* const operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (operation != NULL) {
        status = KineticPDU_GetStatus(operation->response);
    }

    return status;
}

void KineticOperation_BuildNoop(KineticOperation* const operation)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_NOOP;
    operation->request->proto->command->header->has_messageType = true;

    operation->request->entry.value = BYTE_BUFFER_NONE;
    operation->response->entry.value = BYTE_BUFFER_NONE;
}

void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_PUT;
    operation->request->proto->command->header->has_messageType = true;
    operation->request->entry = *entry;
    operation->response->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    operation->request->entry.value = entry->value;
    operation->response->entry.value = BYTE_BUFFER_NONE;
}

void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_GET;
    operation->request->proto->command->header->has_messageType = true;
    operation->request->entry = *entry;
    operation->response->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    operation->request->entry.value = BYTE_BUFFER_NONE;
    operation->response->entry.value = BYTE_BUFFER_NONE;
    if (!entry->metadataOnly) {
        operation->response->entry.value = entry->value;
    }
}

void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_DELETE;
    operation->request->proto->command->header->has_messageType = true;
    operation->request->entry = *entry;
    operation->response->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    operation->request->entry.value = BYTE_BUFFER_NONE;
    operation->response->entry.value = BYTE_BUFFER_NONE;
}

void KineticOperation_BuildGetKeyRange(KineticOperation* const operation,
    KineticKeyRange* range)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_GETKEYRANGE;
    operation->request->proto->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&operation->request->protoData.message, range);
}
