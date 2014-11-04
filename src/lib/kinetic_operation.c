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
#include "kinetic_nbo.h"
#include "kinetic_socket.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <sys/time.h>

static void KineticOperation_ValidateOperation(KineticOperation* operation);

KineticStatus KineticOperation_SendRequest(KineticOperation* const operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->connection == operation->connection);
    LOGF1("\nSending PDU via fd=%d", operation->connection->socket);
    KineticStatus status = KINETIC_STATUS_INVALID;
    KineticPDU* request = operation->request;
    request->proto = &operation->request->protoData.message.message;

    // Pack the command, if available
    if (request->protoData.message.has_command) {
        size_t expectedLen = KineticProto_command__get_packed_size(&request->protoData.message.command);
        request->protoData.message.message.commandBytes.data = (uint8_t*)malloc(expectedLen);
        assert(request->protoData.message.message.commandBytes.data != NULL);
        size_t packedLen = KineticProto_command__pack(
            &request->protoData.message.command,
            request->protoData.message.message.commandBytes.data);
        assert(packedLen == expectedLen);
        request->protoData.message.message.commandBytes.len = packedLen;
        request->protoData.message.message.has_commandBytes = true;
        KineticLogger_LogByteArray(2, "commandBytes", (ByteArray){
            .data = request->protoData.message.message.commandBytes.data,
            .len = request->protoData.message.message.commandBytes.len,
        });
    }

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(&request->hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&request->hmac, request->proto, request->connection->session.hmacKey);

    // Configure PDU header length fields
    request->header.versionPrefix = 'F';
    request->header.protobufLength = KineticProto_Message__get_packed_size(request->proto);
    if (operation->destEntry != NULL && operation->sendValue) {
        request->header.valueLength = operation->destEntry->value.bytesUsed;
    }
    else
    {
        request->header.valueLength = 0;
    }
    KineticLogger_LogHeader(1, &request->header);

    // Create NBO copy of header for sending
    request->headerNBO.versionPrefix = 'F';
    request->headerNBO.protobufLength = KineticNBO_FromHostU32(request->header.protobufLength);
    request->headerNBO.valueLength = KineticNBO_FromHostU32(request->header.valueLength);

    // Pack and send the PDU header
    ByteBuffer hdr = ByteBuffer_Create(&request->headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    status = KineticSocket_Write(request->connection->socket, &hdr);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to send PDU header!");
        return status;
    }

    // Send the protobuf message
    LOG1("Sending PDU Protobuf:");
    KineticLogger_LogProtobuf(2, request->proto);
    status = KineticSocket_WriteProtobuf(request->connection->socket, request);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to send PDU protobuf message!");
        return status;
    }

    // Send the value/payload, if specified
    if (operation->valueEnabled && operation->sendValue) {
        LOGF1("Sending PDU Value Payload (%zu bytes)", operation->destEntry->value.bytesUsed);
        status = KineticSocket_Write(request->connection->socket, &operation->destEntry->value);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG0("Failed to send PDU value payload!");
            return status;
        }
    }

    LOG2("PDU sent successfully!");
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

KineticOperation* KineticOperation_AssociateResponseWithOperation(KineticPDU* response)
{
    if (response == NULL ||
        response->command == NULL ||
        response->command->header == NULL ||
        !response->command->header->has_ackSequence ||
        response->type != KINETIC_PDU_TYPE_RESPONSE)
    {
        LOG0("Response to associate with request is invalid!");
        return NULL;
    }

    const int64_t targetSequence = response->command->header->ackSequence;
    KineticOperation* operation = KineticAllocator_GetFirstOperation(response->connection);
    if (operation == NULL) {
        LOG2("ERROR: No pending operations found!");
        return NULL;
    }

    while (operation != NULL) {
        LOGF2("Comparing received PDU w/ ackSequence=%lld with request with sequence=%lld",
            targetSequence, operation->request->command->header->sequence);
        if (operation->request != NULL &&
            operation->request->type == KINETIC_PDU_TYPE_REQUEST &&
            operation->request->command != NULL &&
            operation->request->command->header != NULL &&
            operation->request->command->header->has_sequence && 
            operation->request->command->header->sequence == targetSequence)
        {
            operation->receiveComplete = false;
            operation->response = response;
            return operation;
        }
        operation = KineticAllocator_GetNextOperation(response->connection, operation);
    }

    return NULL;
}

KineticStatus KineticOperation_ReceiveAsync(KineticOperation* const operation)
{
    assert(operation != NULL);
    assert(operation->request != NULL);
    assert(operation->request->connection != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->command != NULL);
    assert(operation->request->command->header != NULL);
    assert(operation->request->command->header->has_sequence);

    const int fd = operation->request->connection->socket;
    assert(fd >= 0);
    LOGF1("\nReceiving PDU via fd=%d", fd);

    KineticStatus status = KINETIC_STATUS_SUCCESS;

    // Wait for response if no callback supplied (synchronous)
    if (operation->closure.callback == NULL) { 
        bool timeout = false;
        struct timeval tv;
        time_t startTime, currentTime;
        status = KINETIC_STATUS_SOCKET_TIMEOUT;

        // Wait for matching response to arrive
        gettimeofday(&tv, NULL);
        startTime = tv.tv_sec;
        while(!operation->receiveComplete && !timeout) {
            gettimeofday(&tv, NULL);
            currentTime = tv.tv_sec;
            if ((currentTime - startTime) >= KINETIC_PDU_RECEIVE_TIMEOUT_SECS) {
                timeout = true;
            }
            else {
                sleep(0);
            }
        }

        if (timeout) {
            LOG0("Timed out waiting to received response PDU!");
            status = KINETIC_STATUS_SOCKET_TIMEOUT;
        }
        else if (operation->response != NULL) {
            status = KineticPDU_GetStatus(operation->response);
            LOGF2("Response PDU received w/status %s", Kinetic_GetStatusDescription(status));
        }
        else {
            LOG0("Unknown error occurred waiting for response PDU to arrive!");
            status = KINETIC_STATUS_CONNECTION_ERROR;
        }

        KineticAllocator_FreeOperation(operation->connection, operation);
    }

    return status;
}



KineticStatus KineticOperation_NoopCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("NOOP callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildNoop(KineticOperation* const operation)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);
    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP;
    operation->request->protoData.message.command.header->has_messageType = true;

    operation->entryEnabled = false;
    operation->valueEnabled = false;
    operation->sendValue = true;
    operation->callback = &KineticOperation_NoopCallback;
}

KineticStatus KineticOperation_PutCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("PUT callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->entryEnabled);

    // Propagate newVersion to dbVersion in metadata, if newVersion specified
    KineticEntry* entry = operation->destEntry;
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
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->destEntry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, operation->destEntry);

    operation->entryEnabled = true;
    operation->valueEnabled = !operation->destEntry->metadataOnly;
    operation->sendValue = true;
    operation->callback = &KineticOperation_PutCallback;
}

KineticStatus KineticOperation_GetCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("GET callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->entryEnabled);

    // Update the entry upon success
    KineticProto_Command_KeyValue* keyValue = KineticPDU_GetKeyValue(operation->response);
    if (keyValue != NULL) {
        if (!Copy_KineticProto_Command_KeyValue_to_KineticEntry(keyValue, operation->destEntry)) {
            return KINETIC_STATUS_BUFFER_OVERRUN;
        }
    }
    // if (operation->destEntry != NULL) {
        // operation->destEntry->value.bytesUsed = operation->destEntry->value.bytesUsed;
    // }
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->destEntry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    if (operation->destEntry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->destEntry->value);
    }

    operation->entryEnabled = true;
    operation->valueEnabled = !entry->metadataOnly;
    operation->sendValue = false;
    operation->callback = &KineticOperation_GetCallback;
}

KineticStatus KineticOperation_DeleteCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("DELETE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->entryEnabled);
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->destEntry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, operation->destEntry);

    if (operation->destEntry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->destEntry->value);
    }

    operation->entryEnabled = true;
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_DeleteCallback;
}

KineticStatus KineticOperation_GetKeyRangeCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("GETKEYRANGE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->buffers != NULL);
    assert(operation->buffers->count > 0);

    // Report the key list upon success
    KineticProto_Command_Range* keyRange = KineticPDU_GetKeyRange(operation->response);
    if (keyRange != NULL) {
        if (!Copy_KineticProto_Command_Range_to_ByteBufferArray(keyRange, operation->buffers)) {
            return KINETIC_STATUS_BUFFER_OVERRUN;
        }
    }
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildGetKeyRange(KineticOperation* const operation,
    KineticKeyRange* range, ByteBufferArray* buffers)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);
    assert(range != NULL);
    assert(buffers != NULL);

    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE;
    operation->request->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&operation->request->protoData.message, range);

    operation->entryEnabled = false;
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->buffers = buffers;
    operation->callback = &KineticOperation_GetKeyRangeCallback;
}


static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->protoData.message.has_command);
    assert(operation->request->protoData.message.command.header != NULL);
}
