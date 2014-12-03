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
#include "kinetic_device_info.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

static void KineticOperation_ValidateOperation(KineticOperation* operation);

static KineticStatus WritePDU(KineticOperation* const operation)
{
    KineticPDU* request = operation->request;
    // Pack and send the PDU header
    ByteBuffer hdr = ByteBuffer_Create(&request->headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    KineticStatus status = KineticSocket_Write(request->connection->socket, &hdr);
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
        LOGF1("Sending PDU Value Payload (%zu bytes)", operation->entry->value.bytesUsed);
        status = KineticSocket_Write(request->connection->socket, &operation->entry->value);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG0("Failed to send PDU value payload!");
            return status;
        }
    }

    LOG2("PDU sent successfully!");
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticOperation_SendRequest(KineticOperation* const operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->connection == operation->connection);
    LOGF1("\nSending PDU via fd=%d", operation->connection->socket);
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
    if (operation->entry != NULL && operation->sendValue) {
        request->header.valueLength = operation->entry->value.bytesUsed;
    }
    else {
        request->header.valueLength = 0;
    }
    KineticLogger_LogHeader(1, &request->header);

    // Create NBO copy of header for sending
    request->headerNBO.versionPrefix = 'F';
    request->headerNBO.protobufLength = KineticNBO_FromHostU32(request->header.protobufLength);
    request->headerNBO.valueLength = KineticNBO_FromHostU32(request->header.valueLength);

    pthread_mutex_lock(&operation->connection->writeMutex);
    KineticOperation_SetTimeoutTime(operation, KINETIC_OPERATION_TIMEOUT_SECS);
    KineticStatus status = WritePDU(operation);
    pthread_mutex_unlock(&operation->connection->writeMutex);
    return status;
}

KineticStatus KineticOperation_GetStatus(const KineticOperation* const operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;
    if (operation != NULL) {
        status = KineticPDU_GetStatus(operation->response);
    }
    return status;
}

struct timeval KineticOperation_GetTimeoutTime(KineticOperation* const operation)
{
    pthread_mutex_lock(&operation->timeoutTimeMutex);
    struct timeval timeoutTime = operation->timeoutTime;
    pthread_mutex_unlock(&operation->timeoutTimeMutex);
    return timeoutTime;
}

void KineticOperation_SetTimeoutTime(KineticOperation* const operation, uint32_t const timeout_in_sec)
{
    pthread_mutex_lock(&operation->timeoutTimeMutex);

    // set timeout time
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    struct timeval timeoutIn = {
        .tv_sec = timeout_in_sec,
        .tv_usec = 0,
    };
    operation->timeoutTime = Kinetic_TimevalAdd(currentTime, timeoutIn);

    pthread_mutex_unlock(&operation->timeoutTimeMutex);
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
            operation->response = response;
            return operation;
        }
        operation = KineticAllocator_GetNextOperation(response->connection, operation);
    }

    return NULL;
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
    KineticSession_IncrementSequence(&operation->connection->session);
    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_NoopCallback;
}

KineticStatus KineticOperation_PutCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("PUT callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->response != NULL);
    assert(operation->entry != NULL);

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
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticSession_IncrementSequence(&operation->connection->session);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, operation->entry);

    operation->valueEnabled = !operation->entry->metadataOnly;
    operation->sendValue = true;
    operation->callback = &KineticOperation_PutCallback;
}

KineticStatus KineticOperation_GetCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("GET callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->response != NULL);
    assert(operation->entry != NULL);

    // Update the entry upon success
    KineticProto_Command_KeyValue* keyValue = KineticPDU_GetKeyValue(operation->response);
    if (keyValue != NULL) {
        if (!Copy_KineticProto_Command_KeyValue_to_KineticEntry(keyValue, operation->entry)) {
            return KINETIC_STATUS_BUFFER_OVERRUN;
        }
    }

    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticSession_IncrementSequence(&operation->connection->session);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, entry);

    if (operation->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry->value);
    }

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
    assert(operation->response != NULL);
    assert(operation->entry != NULL);
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);
    KineticSession_IncrementSequence(&operation->connection->session);

    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, operation->entry);

    if (operation->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry->value);
    }

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
    assert(operation->response != NULL);
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
    KineticOperation_ValidateOperation(operation);
    assert(range != NULL);
    assert(buffers != NULL);
    KineticSession_IncrementSequence(&operation->connection->session);

    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE;
    operation->request->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&operation->request->protoData.message, range);

    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->buffers = buffers;
    operation->callback = &KineticOperation_GetKeyRangeCallback;
}

KineticStatus KineticOperation_GetLogCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->deviceInfo != NULL);
    LOGF3("GETLOG callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->response != NULL);
    assert(operation->response->command->body->getLog != NULL);

    // Copy the data from the response protobuf into a new info struct
    if (operation->response->command->body->getLog == NULL) {
        return KINETIC_STATUS_OPERATION_FAILED;
    }
    else {
        *operation->deviceInfo = KineticDeviceInfo_Create(operation->response->command->body->getLog);
        return KINETIC_STATUS_SUCCESS;
    }
}

void KineticOperation_BuildGetLog(KineticOperation* const operation,
    KineticDeviceInfo_Type type,
    KineticDeviceInfo** info)
{
    KineticOperation_ValidateOperation(operation);
    KineticSession_IncrementSequence(&operation->connection->session);
    KineticProto_Command_GetLog_Type protoType =
        KineticDeviceInfo_Type_to_KineticProto_Command_GetLog_Type(type);
        
    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETLOG;
    operation->request->command->header->has_messageType = true;
    operation->request->command->body = &operation->request->protoData.message.body;
    operation->request->command->body->getLog = &operation->request->protoData.message.getLog;
    operation->request->command->body->getLog->types = &operation->request->protoData.message.getLogType;
    operation->request->command->body->getLog->types[0] = protoType;
    operation->request->command->body->getLog->n_types = 1;
    operation->deviceInfo = info;
    operation->callback = &KineticOperation_GetLogCallback;
}

KineticStatus KineticOperation_InstantSecureEraseCallback(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("IntantSecureErase callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildInstantSecureErase(KineticOperation* operation)
{
    KineticOperation_ValidateOperation(operation);
    KineticSession_IncrementSequence(&operation->connection->session);
    operation->request->protoData.message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    operation->request->protoData.message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->protoData.message.body;
    operation->request->command->body->pinOp = &operation->request->protoData.message.pinOp;
    operation->request->command->body->pinOp->pinOpType = KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_SECURE_ERASE_PINOP;
    operation->request->command->body->pinOp->has_pinOpType = true;
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_InstantSecureEraseCallback;
}


static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->connection != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->protoData.message.has_command);
    assert(operation->request->command != NULL);
    assert(operation->request->command->header != NULL);
    assert(operation->request->command->header->has_sequence);
}

void KineticOperation_Complete(KineticOperation* operation, KineticStatus status)
{
    assert(operation != NULL);
    // ExecuteOperation should ensure a callback exists (either a user supplied one, or the a default)
    assert(operation->closure.callback != NULL);
    KineticCompletionData completionData = {.status = status};
    operation->closure.callback(&completionData, operation->closure.clientData);
    KineticAllocator_FreeOperation(operation->connection, operation);
}

void KineticOperation_TimeoutOperations(KineticConnection* const connection)
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    for (KineticOperation* operation = KineticAllocator_GetFirstOperation(connection);
         operation != NULL;
         operation = KineticAllocator_GetNextOperation(connection, operation)) {

        struct timeval timeoutTime = KineticOperation_GetTimeoutTime(operation);

        // if this operation has a nonzero timeout
        //   and it's timed out
        if (!Kinetic_TimevalIsZero(timeoutTime) &&
            Kinetic_TimevalCmp(currentTime, operation->timeoutTime) >= 0)
        {
            KineticOperation_Complete(operation, KINETIC_STATUS_OPERATION_TIMEDOUT);
        }
    }
}
