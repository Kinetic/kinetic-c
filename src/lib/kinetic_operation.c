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
#include "kinetic_controller.h"
#include "kinetic_session.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_nbo.h"
#include "kinetic_socket.h"
#include "kinetic_device_info.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "bus.h"

static void KineticOperation_ValidateOperation(KineticOperation* operation);
static KineticStatus KineticOperation_SendRequestInner(KineticOperation* const operation);

KineticStatus KineticOperation_SendRequest(KineticOperation* const operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);

    KineticCountingSemaphore_Take(operation->connection->outstandingOperations);
    KineticStatus status = KineticOperation_SendRequestInner(operation);
    if (status != KINETIC_STATUS_SUCCESS)
    {
        //cleanup
        KineticPDU* request = operation->request;
        if (request->message.message.commandBytes.data != NULL) {
            free(request->message.message.commandBytes.data);
            request->message.message.commandBytes.data = NULL;
        }
        KineticCountingSemaphore_Give(operation->connection->outstandingOperations);
    }
    return status;
}

// TODO: Asses refactoring this methog by disecting out Operation and relocate to kinetic_pdu
static KineticStatus KineticOperation_SendRequestInner(KineticOperation* const operation)
{
    LOGF3("\nSending PDU via fd=%d", operation->connection->messageBus);

    KineticStatus status = KINETIC_STATUS_INVALID;
    uint8_t * msg = NULL;
    KineticPDU* request = operation->request;
    KineticProto_Message* proto = &operation->request->message.message;
    pthread_mutex_t* sendMutex = &operation->connection->sendMutex;

    // Acquire lock
    pthread_mutex_lock(sendMutex);

    // Pack the command, if available
    size_t expectedLen = KineticProto_command__get_packed_size(&request->message.command);
    request->message.message.commandBytes.data = (uint8_t*)malloc(expectedLen);
    if(request->message.message.commandBytes.data == NULL)
    {
        LOG0("Failed to allocate command bytes!");
        status = KINETIC_STATUS_MEMORY_ERROR;
        goto cleanup;
    }
    size_t packedLen = KineticProto_command__pack(
        &request->message.command,
        request->message.message.commandBytes.data);
    assert(packedLen == expectedLen);
    request->message.message.commandBytes.len = packedLen;
    request->message.message.has_commandBytes = true;
    KineticLogger_LogByteArray(3, "commandBytes", (ByteArray){
        .data = request->message.message.commandBytes.data,
        .len = request->message.message.commandBytes.len,
    });

    switch (proto->authType) {
    case KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH:
        /* TODO: If operation uses PIN AUTH, then init that */
        break;
    case KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH:
        {
            KineticHMAC hmac;

            // Populate the HMAC for the protobuf
            KineticHMAC_Init(&hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
            KineticHMAC_Populate(&hmac, proto, operation->connection->session.config.hmacKey);
        } break;
    default:
        break;
    }

    KineticPDUHeader header;
    memset(&header, 0, sizeof(header));

    // Configure PDU header length fields
    header.versionPrefix = 'F';
    header.protobufLength = KineticProto_Message__get_packed_size(proto);
    KineticLogger_LogProtobuf(3, proto);

    if (operation->entry != NULL && operation->sendValue) {
        header.valueLength = operation->entry->value.bytesUsed;
    }
    else {
        header.valueLength = 0;
    }

    if (header.valueLength > PDU_PROTO_MAX_LEN) {
        // Packed value exceeds max size.
        LOGF2("\nPacked value exceeds maximum size. Packed size is: %d, Max size is: %d", header.valueLength, PDU_PROTO_MAX_LEN);
        status = KINETIC_STATUS_BUFFER_OVERRUN;
        goto cleanup;
    }

    // Populate sequence count and increment it for next operation
    request->message.header.sequence = operation->connection->sequence++;

    LOGF1("[PDU TX] pdu: 0x%0llX, op: 0x%llX, session: 0x%llX, bus: 0x%llX, seq: %5lld, protoLen: %4u, valueLen: %u",
        operation->request, operation, &operation->connection->session, operation->connection->messageBus,
        request->message.header.sequence, header.protobufLength, header.valueLength);

    KineticLogger_LogHeader(3, &header);

    uint32_t nboProtoLength = KineticNBO_FromHostU32(header.protobufLength);
    uint32_t nboValueLength = KineticNBO_FromHostU32(header.valueLength);

    size_t offset = 0;
    msg = malloc(PDU_HEADER_LEN + header.protobufLength + header.valueLength);
    if (msg == NULL) {
        LOG0("Failed to allocate outgoing message!");
        status = KINETIC_STATUS_MEMORY_ERROR;
        goto cleanup;
    }
    
    msg[offset] = header.versionPrefix;
    offset += sizeof(header.versionPrefix);

    memcpy(&msg[offset], &nboProtoLength, sizeof(nboProtoLength));
    offset += sizeof(nboProtoLength);
    memcpy(&msg[offset], &nboValueLength, sizeof(nboValueLength));
    offset += sizeof(nboValueLength);

    size_t len = KineticProto_Message__pack(&request->message.message, &msg[offset]);
    assert(len == header.protobufLength);
    offset += header.protobufLength;

    free(request->message.message.commandBytes.data);
    request->message.message.commandBytes.data = NULL;

    // Send the value/payload, if specified
    if (header.valueLength > 0) {
        memcpy(&msg[offset], operation->entry->value.array.data, operation->entry->value.bytesUsed);
        offset += operation->entry->value.bytesUsed;
    }
    assert((PDU_HEADER_LEN + header.protobufLength + header.valueLength) == offset);

    bus_send_request(operation->connection->messageBus, &(bus_user_msg){
        .fd       = operation->connection->socket,
        .type     = BUS_SOCKET_PLAIN,
        .seq_id   = request->message.header.sequence,
        .msg      = msg,
        .msg_size = offset,
        .cb       = KineticController_HandleExpectedResponse,
        .udata    = operation,
    });

    status = KINETIC_STATUS_SUCCESS;

cleanup:

    pthread_mutex_unlock(sendMutex);

    if (msg != NULL) { free(msg); }

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

KineticStatus KineticOperation_NoopCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("NOOP callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return status;
}

void KineticOperation_BuildNoop(KineticOperation* const operation)
{
    KineticOperation_ValidateOperation(operation);
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP;
    operation->request->message.command.header->has_messageType = true;
    operation->valueEnabled = false;
    // ######## TODO should be able to remove sendvalue
    operation->sendValue = false;
    operation->callback = &KineticOperation_NoopCallback;
}

KineticStatus KineticOperation_PutCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("PUT callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->entry != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        assert(operation->response != NULL);
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

void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);

    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT;
    operation->request->message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->message, operation->entry);

    operation->valueEnabled = !operation->entry->metadataOnly;
    operation->sendValue = true;
    operation->callback = &KineticOperation_PutCallback;
}

static KineticStatus get_cb(const char *cmd_name, KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("%s callback w/ operation (0x%0llX) on connection (0x%0llX)",
        cmd_name, operation, operation->connection);
    assert(operation->entry != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        assert(operation->response != NULL);
        // Update the entry upon success
        KineticProto_Command_KeyValue* keyValue = KineticPDU_GetKeyValue(operation->response);
        if (keyValue != NULL) {
            if (!Copy_KineticProto_Command_KeyValue_to_KineticEntry(keyValue, operation->entry)) {
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

static void build_get_command(KineticOperation* const operation,
                              KineticEntry* const entry,
                              KineticOperationCallback cb,
                              KineticProto_Command_MessageType command_id)
{
    KineticOperation_ValidateOperation(operation);

    operation->request->message.command.header->messageType = command_id;
    operation->request->message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->message, entry);

    if (operation->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry->value);
    }

    operation->valueEnabled = !entry->metadataOnly;
    operation->sendValue = false;
    operation->callback = cb;
}

static KineticStatus get_cmd_cb(KineticOperation* const operation, KineticStatus const status)
{
    return get_cb("GET", operation, status);
}

void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry)
{
    build_get_command(operation, entry, &get_cmd_cb,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET);
}

static KineticStatus getprevious_cmd_cb(KineticOperation* const operation, KineticStatus const status)
{
    return get_cb("GETPREVIOUS", operation, status);
}

void KineticOperation_BuildGetPrevious(KineticOperation* const operation,
                                   KineticEntry* const entry)
{
    build_get_command(operation, entry, &getprevious_cmd_cb,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETPREVIOUS);
}

static KineticStatus getnext_cmd_cb(KineticOperation* const operation, KineticStatus const status)
{
    return get_cb("GETNEXT", operation, status);
}

void KineticOperation_BuildGetNext(KineticOperation* const operation,
                                   KineticEntry* const entry)
{
    build_get_command(operation, entry, &getnext_cmd_cb,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETNEXT);
}

KineticStatus KineticOperation_FlushCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("FLUSHALLDATA callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);

    return status;
}

void KineticOperation_BuildFlush(KineticOperation* const operation)
{
    KineticOperation_ValidateOperation(operation);
    operation->request->message.command.header->messageType =
      KINETIC_PROTO_COMMAND_MESSAGE_TYPE_FLUSHALLDATA;
    operation->request->message.command.header->has_messageType = true;
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_FlushCallback;
}

KineticStatus KineticOperation_DeleteCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("DELETE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->entry != NULL);
    return status;
}

void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(operation);

    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE;
    operation->request->message.command.header->has_messageType = true;
    operation->entry = entry;

    KineticMessage_ConfigureKeyValue(&operation->request->message, operation->entry);

    if (operation->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&operation->entry->value);
    }

    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_DeleteCallback;
}

KineticStatus KineticOperation_GetKeyRangeCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("GETKEYRANGE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    assert(operation->buffers != NULL);
    assert(operation->buffers->count > 0);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        assert(operation->response != NULL);
        // Report the key list upon success
        KineticProto_Command_Range* keyRange = KineticPDU_GetKeyRange(operation->response);
        if (keyRange != NULL) {
            if (!Copy_KineticProto_Command_Range_to_ByteBufferArray(keyRange, operation->buffers)) {
                return KINETIC_STATUS_BUFFER_OVERRUN;
            }
        }
    }
    return status;
}

void KineticOperation_BuildGetKeyRange(KineticOperation* const operation,
    KineticKeyRange* range, ByteBufferArray* buffers)
{
    KineticOperation_ValidateOperation(operation);
    assert(range != NULL);
    assert(buffers != NULL);

    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE;
    operation->request->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&operation->request->message, range);

    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->buffers = buffers;
    operation->callback = &KineticOperation_GetKeyRangeCallback;
}

KineticStatus KineticOperation_GetLogCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->deviceInfo != NULL);
    LOGF3("GETLOG callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        assert(operation->response != NULL);
        // Copy the data from the response protobuf into a new info struct
        if (operation->response->command->body->getLog == NULL) {
            return KINETIC_STATUS_OPERATION_FAILED;
        }
        else {
            *operation->deviceInfo = KineticDeviceInfo_Create(operation->response->command->body->getLog);
            return KINETIC_STATUS_SUCCESS;
        }
    }
    return status;
}

void KineticOperation_BuildGetLog(KineticOperation* const operation,
    KineticDeviceInfo_Type type,
    KineticDeviceInfo** info)
{
    KineticOperation_ValidateOperation(operation);
    KineticProto_Command_GetLog_Type protoType =
        KineticDeviceInfo_Type_to_KineticProto_Command_GetLog_Type(type);
        
    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETLOG;
    operation->request->command->header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    operation->request->command->body->getLog = &operation->request->message.getLog;
    operation->request->command->body->getLog->types = &operation->request->message.getLogType;
    operation->request->command->body->getLog->types[0] = protoType;
    operation->request->command->body->getLog->n_types = 1;
    operation->deviceInfo = info;
    operation->callback = &KineticOperation_GetLogCallback;
}


void destroy_p2pOp(KineticProto_Command_P2POperation* proto_p2pOp)
{
    if (proto_p2pOp != NULL) {
        if (proto_p2pOp->peer != NULL) {
            free(proto_p2pOp->peer);
            proto_p2pOp->peer = NULL;
        }
        if (proto_p2pOp->operation != NULL) {
            for(size_t i = 0; i < proto_p2pOp->n_operation; i++) {
                if (proto_p2pOp->operation[i] != NULL) {
                    if (proto_p2pOp->operation[i]->p2pop != NULL) {
                        destroy_p2pOp(proto_p2pOp->operation[i]->p2pop);
                        proto_p2pOp->operation[i]->p2pop = NULL;
                    }
                    if (proto_p2pOp->operation[i]->status != NULL) {
                        free(proto_p2pOp->operation[i]->status);
                        proto_p2pOp->operation[i]->status = NULL;
                    }
                    free(proto_p2pOp->operation[i]);
                    proto_p2pOp->operation[i] = NULL;
                }
            }
            free(proto_p2pOp->operation);
            proto_p2pOp->operation = NULL;
        }
        free(proto_p2pOp);
    }
}


KineticProto_Command_P2POperation* build_p2pOp(uint32_t nestingLevel, KineticP2P_Operation const * const p2pOp)
{
    // limit nesting level to 10000
    if (nestingLevel == 1000) {
        LOG0("P2P operation nesting level is too deep. Max is 1000.");
        return NULL;
    }

    KineticProto_Command_P2POperation* proto_p2pOp = calloc(1, sizeof(KineticProto_Command_P2POperation));
    if (proto_p2pOp == NULL) { goto error_cleanup; }

    KineticProto_command_p2_poperation__init(proto_p2pOp);

    proto_p2pOp->peer = calloc(1, sizeof(KineticProto_Command_P2POperation_Peer));
    if (proto_p2pOp->peer == NULL) { goto error_cleanup; }

    KineticProto_command_p2_poperation_peer__init(proto_p2pOp->peer);

    proto_p2pOp->peer->hostname = p2pOp->peer.hostname;
    proto_p2pOp->peer->has_port = true;
    proto_p2pOp->peer->port = p2pOp->peer.port;
    proto_p2pOp->peer->has_tls = true;
    proto_p2pOp->peer->tls = p2pOp->peer.tls;

    proto_p2pOp->n_operation = p2pOp->numOperations;
    proto_p2pOp->operation = calloc(p2pOp->numOperations, sizeof(KineticProto_Command_P2POperation_Operation*));
    if (proto_p2pOp->operation == NULL) { goto error_cleanup; }

    for(size_t i = 0; i < proto_p2pOp->n_operation; i++) {
        assert(!ByteBuffer_IsNull(p2pOp->operations[i].key)); // TODO return invalid operand?
        
        KineticProto_Command_P2POperation_Operation * p2p_op_op = calloc(1, sizeof(KineticProto_Command_P2POperation_Operation));
        if (p2p_op_op == NULL) { goto error_cleanup; }

        KineticProto_command_p2_poperation_operation__init(p2p_op_op);

        p2p_op_op->has_key = true;
        p2p_op_op->key.data = p2pOp->operations[i].key.array.data;
        p2p_op_op->key.len = p2pOp->operations[i].key.bytesUsed;

        p2p_op_op->has_newKey = !ByteBuffer_IsNull(p2pOp->operations[i].newKey);
        p2p_op_op->newKey.data = p2pOp->operations[i].newKey.array.data;
        p2p_op_op->newKey.len = p2pOp->operations[i].newKey.bytesUsed;

        p2p_op_op->has_version = !ByteBuffer_IsNull(p2pOp->operations[i].version);
        p2p_op_op->version.data = p2pOp->operations[i].version.array.data;
        p2p_op_op->version.len = p2pOp->operations[i].version.bytesUsed;

        // force if no version was specified
        p2p_op_op->has_force = ByteBuffer_IsNull(p2pOp->operations[i].version);
        p2p_op_op->force = ByteBuffer_IsNull(p2pOp->operations[i].version);

        if (p2pOp->operations[i].chainedOperation == NULL) {
            p2p_op_op->p2pop = NULL;
        } else {
            p2p_op_op->p2pop = build_p2pOp(nestingLevel + 1, p2pOp->operations[i].chainedOperation);
            if (p2p_op_op->p2pop == NULL) { goto error_cleanup; }
        }

        p2p_op_op->status = NULL;

        proto_p2pOp->operation[i] = p2p_op_op;
    }
    return proto_p2pOp;

error_cleanup:
    destroy_p2pOp(proto_p2pOp);
    return NULL;
}

static void populateP2PStatusCodes(KineticP2P_Operation* const p2pOp, KineticProto_Command_P2POperation const * const p2pOperation)
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


KineticStatus KineticOperation_P2POperationCallback(KineticOperation* const operation, KineticStatus const status)
{
    KineticP2P_Operation* const p2pOp = operation->p2pOp;

    if (status == KINETIC_STATUS_SUCCESS)
    {
        if ((operation->response != NULL) &&
            (operation->response->command != NULL) &&
            (operation->response->command->body != NULL) &&
            (operation->response->command->body->p2pOperation != NULL)) {
            populateP2PStatusCodes(p2pOp, operation->response->command->body->p2pOperation);
        }
    }

    destroy_p2pOp(operation->request->command->body->p2pOperation);

    return status;
}

KineticStatus KineticOperation_BuildP2POperation(KineticOperation* const operation,
                                                 KineticP2P_Operation* const p2pOp)
{
    KineticOperation_ValidateOperation(operation);
        
    operation->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PEER2PEERPUSH;
    operation->request->command->header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;

    operation->request->command->body->p2pOperation = build_p2pOp(0, p2pOp);
    
    if (operation->request->command->body->p2pOperation == NULL) {
        return KINETIC_STATUS_OPERATION_INVALID;
    }

    if (p2pOp->numOperations >= 100000) {
        return KINETIC_STATUS_BUFFER_OVERRUN;
    }

    operation->p2pOp = p2pOp;
    operation->callback = &KineticOperation_P2POperationCallback;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticOperation_InstantSecureEraseCallback(KineticOperation* const operation, KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("IntantSecureErase callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return status;
}

void KineticOperation_BuildInstantSecureErase(KineticOperation* operation)
{
    KineticOperation_ValidateOperation(operation);
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    operation->request->message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    operation->request->command->body->pinOp = &operation->request->message.pinOp;
 
#if 0
    /* Replace HMAC auth with pin auth */
    KineticProto_Message* pdu = operation->request->proto;
    pdu->has_authType = true;
    pdu->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH;
    //ProtobufCBinaryData pin_bd = { .data="pin", .len=3 };
    ProtobufCBinaryData pin_bd = { .data="", .len=0 };

    pdu->hmacAuth = NULL;
#if 0
    pdu->hmacAuth->has_hmac = false;
    pdu->hmacAuth->has_identity = false;
    pdu->hmacAuth->hmac = (ProtobufCBinaryData) {
        .data = NULL, .len = 0,
    };
#endif

    pdu->pinAuth = &operation->request->message.pinAuth;
    pdu->pinAuth->pin = pin_bd;
    pdu->pinAuth->has_pin = true;
#endif
    
    operation->request->command->body->pinOp->pinOpType = KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_SECURE_ERASE_PINOP;
    //operation->request->command->body->pinOp->pinOpType = KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_ERASE_PINOP;
    operation->request->command->body->pinOp->has_pinOpType = true;
    
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_InstantSecureEraseCallback;
}

KineticStatus KineticOperation_SetClusterVersionCallback(KineticOperation* operation,
    KineticStatus const status)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    LOGF3("SetClusterVersion callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    (void)status;
    return KINETIC_STATUS_SUCCESS;
}

void KineticOperation_BuildSetClusterVersion(KineticOperation* operation, int64_t newClusterVersion)
{
    KineticOperation_ValidateOperation(operation);
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    operation->request->message.command.header->has_messageType = true;
    
    operation->request->command->body->setup->newClusterVersion = newClusterVersion;
    operation->request->command->body->setup->has_newClusterVersion = true;

    operation->request->command->body = &operation->request->message.body;

    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_SetClusterVersionCallback;
}

static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->command != NULL);
    assert(operation->request->command->header != NULL);
    assert(operation->request->command->header->has_sequence);
}

void KineticOperation_Complete(KineticOperation* operation, KineticStatus status)
{
    assert(operation != NULL);
    // ExecuteOperation should ensure a callback exists (either a user supplied one, or the a default)
    KineticCompletionData completionData = {.status = status};

    KineticCountingSemaphore_Give(operation->connection->outstandingOperations);

    if(operation->closure.callback != NULL) {
        operation->closure.callback(&completionData, operation->closure.clientData);
    }

    KineticAllocator_FreeOperation(operation);
}
