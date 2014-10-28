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

static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->protoData.message.has_command);
    assert(operation->request->protoData.message.command.header != NULL);
}

KineticOperation KineticOperation_Create(KineticConnection* const connection)
{
    LOGF1("\n"
         "--------------------------------------------------\n"
         "Building new operation on connection @ 0x%llX", connection);

    KineticOperation operation = {
        .connection = connection,
        .request = KineticAllocator_NewPDU(connection),
        .response = NULL,
    };

    if (operation.request == NULL) {
        LOG0("Request PDU could not be allocated! Try reusing or freeing a PDU.");
        return (KineticOperation) {.request = NULL, .response = NULL};
    }

    KineticPDU_Init(operation.request, connection);
    KINETIC_PDU_INIT_WITH_COMMAND(operation.request, connection);

    return operation;
}

KineticStatus KineticOperation_Free(KineticOperation* const operation)
{
    if (operation == NULL) {
        LOG1("Specified operation is NULL so nothing to free");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (operation->request != NULL) {
        KineticAllocator_FreePDU(operation->connection, operation->request);
        operation->request = NULL;
    }

    if (operation->response != NULL) {
        KineticAllocator_FreePDU(operation->connection, operation->response);
        operation->response = NULL;
    }

    return KINETIC_STATUS_SUCCESS;
}

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
    request->header.valueLength = (operation->sendValue) ? operation->entry.value.bytesUsed : 0;
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
        LOGF1("Sending PDU Value Payload (%zu bytes)", operation->entry.value.bytesUsed);
        status = KineticSocket_Write(request->connection->socket, &operation->entry.value);
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
    while (operation != NULL) {
        if (operation->request != NULL &&
            operation->request->type == KINETIC_PDU_TYPE_REQUEST &&
            operation->request->command != NULL &&
            operation->request->command->header != NULL &&
            operation->request->command->header->has_sequence && 
            operation->request->command->header->sequence == targetSequence)
        {
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
    assert(operation->response != NULL);

    const int fd = operation->request->connection->socket;
    assert(fd >= 0);
    LOGF1("\nReceiving PDU (asynchronous) via fd=%d", fd);

    bool timeout = false;
    struct timeval tv;
    time_t startTime;
    time_t currentTime;
    KineticStatus status = KINETIC_STATUS_SOCKET_TIMEOUT;

    // Obtain start time
    gettimeofday(&tv, NULL);
    startTime = tv.tv_sec;

    // Wait for matching response to arrive
    while(operation->response == NULL && !timeout) {
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
        if (status == KINETIC_STATUS_SUCCESS) {
            LOG2("PDU received successfully!");
        }
    }
    else {
        LOG0("Unknown error occurred waiting for response PDU to arrive!");
        status = KINETIC_STATUS_CONNECTION_ERROR;
    }

    return status;
}





void KineticOperation_BuildNoop(KineticOperation* const operation)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);
    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry.value = BYTE_BUFFER_NONE;
}

void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);
}

void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    if (operation->entry.value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry.value);
    }

    operation->entryEnabled = true;
    operation->valueEnabled = !entry->metadataOnly;
    operation->sendValue = false;
}

void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = *entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    if (operation->entry.value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry.value);
    }

    operation->entryEnabled = true;
    operation->valueEnabled = false;
    operation->sendValue = false;
}

void KineticOperation_BuildGetKeyRange(KineticOperation* const operation,
    KineticKeyRange* range)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE;
    operation->request->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&operation->request->protoData.message, range);

    operation->entryEnabled = false;
    operation->valueEnabled = false;
    operation->sendValue = false;
}
