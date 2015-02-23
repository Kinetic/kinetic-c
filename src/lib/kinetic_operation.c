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
#include "kinetic_auth.h"
#include "kinetic_socket.h"
#include "kinetic_device_info.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "bus.h"

#define ATOMIC_FETCH_AND_INCREMENT(P) __sync_fetch_and_add(P, 1)

static void KineticOperation_ValidateOperation(KineticOperation* operation);
static KineticStatus KineticOperation_SendRequestInner(KineticOperation* const operation);

KineticStatus KineticOperation_SendRequest(KineticOperation* const operation)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    KINETIC_ASSERT(operation->request != NULL);
    
    KineticStatus status = KineticOperation_SendRequestInner(operation);
    if (status != KINETIC_STATUS_SUCCESS)
    {
        //cleanup
        KineticPDU* request = operation->request;
        if (request != NULL) {
            if (request->message.message.commandBytes.data != NULL) {
                free(request->message.message.commandBytes.data);
                request->message.message.commandBytes.data = NULL;
            }
            free(request);
            request = NULL;
        }
    }

    return status;
}

static void log_request_seq_id(int fd, int64_t seq_id, KineticMessageType mt)
{
    #if KINETIC_LOGGER_LOG_SEQUENCE_ID
    struct timeval tv;
    gettimeofday(&tv, NULL);
    LOGF2("SEQ_ID request fd %d seq_id %lld %08lld.%08lld cmd %02x",
        fd, (long long)seq_id,
        (long long)tv.tv_sec, (long long)tv.tv_usec, (uint8_t)mt);
    #else
    (void)seq_id;
    (void)mt;
    #endif
}    

// TODO: Assess refactoring this method by dissecting out Operation and relocate to kinetic_pdu
static KineticStatus KineticOperation_SendRequestInner(KineticOperation* const operation)
{
    LOGF3("\nSending PDU via fd=%d", operation->connection->socket);

    KineticStatus status = KINETIC_STATUS_INVALID;
    uint8_t * msg = NULL;
    KineticPDU* request = operation->request;
    KineticProto_Message* proto = &operation->request->message.message;
    pthread_mutex_t* sendMutex = &operation->connection->sendMutex;

    // Acquire lock
    pthread_mutex_lock(sendMutex);
    KineticSession *session = operation->connection->pSession;

    // Populate sequence count and increment it for next operation
    KINETIC_ASSERT(request->message.header.sequence == KINETIC_SEQUENCE_NOT_YET_BOUND);
    int64_t seq_id = KineticSession_GetNextSequenceCount(session);
    request->message.header.sequence = seq_id;

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
    KINETIC_ASSERT(packedLen == expectedLen);
    request->message.message.commandBytes.len = packedLen;
    request->message.message.has_commandBytes = true;
    KineticLogger_LogByteArray(3, "commandBytes", (ByteArray){
        .data = request->message.message.commandBytes.data,
        .len = request->message.message.commandBytes.len,
    });

    log_request_seq_id(operation->connection->socket, seq_id, request->message.header.messageType);

    // Populate the HMAC/PIN for protobuf authentication
    if (operation->pin != NULL) {
        status = KineticAuth_PopulatePin(&session->config, operation->request, *operation->pin);
    }
    else {
        status = KineticAuth_PopulateHmac(&session->config, operation->request);
    }
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed populating authentication info for new request!");
        return status;
    }

    // Configure PDU header length fields
    KineticPDUHeader header = {
        .versionPrefix = 'F',
        .protobufLength = KineticProto_Message__get_packed_size(proto)
    };
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

    uint32_t nboProtoLength = KineticNBO_FromHostU32(header.protobufLength);
    uint32_t nboValueLength = KineticNBO_FromHostU32(header.valueLength);

    // Allocate and pack message
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
    KINETIC_ASSERT(len == header.protobufLength);
    offset += header.protobufLength;

    KINETIC_ASSERT(operation);
    KINETIC_ASSERT(operation->request);
    KINETIC_ASSERT(operation->connection);
    KINETIC_ASSERT(request);

    LOGF2("[PDU TX] pdu: %p, session: %p, bus: %p, "
        "fd: %6d, seq: %8lld, protoLen: %8u, valueLen: %8u, op: %p, msgType: %02x",
        (void*)operation->request,
        (void*)operation->connection->pSession, (void*)operation->connection->messageBus,
        operation->connection->socket, (long long)request->message.header.sequence,
        header.protobufLength, header.valueLength,
        (void*)operation, request->message.header.messageType);
    KineticLogger_LogHeader(3, &header);
    KineticLogger_LogProtobuf(3, proto);

    free(request->message.message.commandBytes.data);
    request->message.message.commandBytes.data = NULL;

    // Send the value/payload, if specified
    if (header.valueLength > 0) {
        memcpy(&msg[offset], operation->entry->value.array.data, operation->entry->value.bytesUsed);
        offset += operation->entry->value.bytesUsed;
    }
    KINETIC_ASSERT((PDU_HEADER_LEN + header.protobufLength + header.valueLength) == offset);

    // Claim a send PDU for proper throttling of concurrent requests to avoid flooding the drive
    KineticCountingSemaphore * const sem = operation->connection->outstandingOperations;
    KineticCountingSemaphore_Take(sem);
    int fd = operation->connection->socket;
    if (!bus_send_request(operation->connection->messageBus, &(bus_user_msg){
        .fd       = fd,
        .type     = BUS_SOCKET_PLAIN,
        .seq_id   = seq_id,
        .msg      = msg,
        .msg_size = offset,
        .cb       = KineticController_HandleResult,
        .udata    = operation,
        .timeout_sec = operation->timeoutSeconds,
        }))
    {
        LOGF0("Failed queuing request %p for transmit on fd=%d w/seq=%lld",
            (void*)request, fd, (long long)seq_id);
        // Since the request PDU failed getting queued into the bus,
        // release this request from the throttle to clean up properly
        KineticCountingSemaphore_Give(sem);
        status = KINETIC_STATUS_SOCKET_ERROR;
    }
    else {
        status = KINETIC_STATUS_SUCCESS;
    }

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

static void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    KINETIC_ASSERT(operation->request != NULL);
    KINETIC_ASSERT(operation->request->command != NULL);
    KINETIC_ASSERT(operation->request->command->header != NULL);
    KINETIC_ASSERT(operation->request->command->header->has_sequence);
}

void KineticOperation_Complete(KineticOperation* operation, KineticStatus status)
{
    KINETIC_ASSERT(operation != NULL);
    // ExecuteOperation should ensure a callback exists (either a user supplied one, or the a default)
    KineticCompletionData completionData = {.status = status};

    // Release this request so that others can be unblocked if at max (request PDUs throttled)
    KineticCountingSemaphore_Give(operation->connection->outstandingOperations);

    if(operation->closure.callback != NULL) {
        operation->closure.callback(&completionData, operation->closure.clientData);
    }

    KineticAllocator_FreeOperation(operation);
}



/*******************************************************************************
 * Client Operations
*******************************************************************************/

KineticStatus KineticOperation_NoopCallback(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("PUT callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("%s callback w/ operation (0x%0llX) on connection (0x%0llX)",
        cmd_name, operation, operation->connection);
    KINETIC_ASSERT(operation->entry != NULL);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("DELETE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    KINETIC_ASSERT(operation->entry != NULL);
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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("GETKEYRANGE callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    KINETIC_ASSERT(operation->buffers != NULL);
    KINETIC_ASSERT(operation->buffers->count > 0);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
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
    KINETIC_ASSERT(range != NULL);
    KINETIC_ASSERT(buffers != NULL);

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
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    KINETIC_ASSERT(operation->deviceInfo != NULL);
    LOGF3("GETLOG callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);

    if (status == KINETIC_STATUS_SUCCESS)
    {
        KINETIC_ASSERT(operation->response != NULL);
        // Copy the data from the response protobuf into a new info struct
        if (operation->response->command->body->getLog == NULL) {
            return KINETIC_STATUS_OPERATION_FAILED;
        }
        else {
            *operation->deviceInfo = KineticLogInfo_Create(operation->response->command->body->getLog);
            return KINETIC_STATUS_SUCCESS;
        }
    }
    return status;
}

void KineticOperation_BuildGetLog(KineticOperation* const operation,
    KineticLogInfo_Type type,
    KineticLogInfo** info)
{
    KineticOperation_ValidateOperation(operation);
    KineticProto_Command_GetLog_Type protoType =
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(type);
        
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
        KINETIC_ASSERT(!ByteBuffer_IsNull(p2pOp->operations[i].key)); // TODO return invalid operand?
        
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



/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/

KineticStatus KineticOperation_SetPinCallback(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("SetPin callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return status;
}

void KineticOperation_BuildSetPin(KineticOperation* const operation, ByteArray old_pin, ByteArray new_pin, bool lock)
{
    KineticOperation_ValidateOperation(operation);

    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SECURITY;
    operation->request->message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    operation->request->command->body->security = &operation->request->message.security;

    if (lock) {
        operation->request->message.security.oldLockPIN = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        operation->request->message.security.has_oldLockPIN = true;
        operation->request->message.security.newLockPIN = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        operation->request->message.security.has_newLockPIN = true;
    }
    else {
        operation->request->message.security.oldErasePIN = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        operation->request->message.security.has_oldErasePIN = true;
        operation->request->message.security.newErasePIN = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        operation->request->message.security.has_newErasePIN = true;
    }
    
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_SetPinCallback;
    operation->request->pinAuth = false;
}

KineticStatus KineticOperation_EraseCallback(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("Erase callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return status;
}

void KineticOperation_BuildErase(KineticOperation* const operation, bool secure_erase, ByteArray* pin)
{
    KineticOperation_ValidateOperation(operation);

    operation->pin = pin;
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP;
    operation->request->message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    operation->request->command->body->pinOp = &operation->request->message.pinOp;
    operation->request->command->body->pinOp->pinOpType = secure_erase ?
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_SECURE_ERASE_PINOP :
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_ERASE_PINOP;
    operation->request->command->body->pinOp->has_pinOpType = true;
    
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_EraseCallback;
    operation->request->pinAuth = true;
    operation->timeoutSeconds = 180;
}

KineticStatus KineticOperation_LockUnlockCallback(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("LockUnlockCallback callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    return status;
}

void KineticOperation_BuildLockUnlock(KineticOperation* const operation, bool lock, ByteArray* pin)
{
    KineticOperation_ValidateOperation(operation);

    operation->pin = pin;
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP;
    operation->request->message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    operation->request->command->body->pinOp = &operation->request->message.pinOp;
    
    operation->request->command->body->pinOp->pinOpType = lock ?
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_LOCK_PINOP :
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_UNLOCK_PINOP;
    operation->request->command->body->pinOp->has_pinOpType = true;
    
    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_LockUnlockCallback;
    operation->request->pinAuth = true;
}

KineticStatus KineticOperation_SetClusterVersionCallback(KineticOperation* const operation, KineticStatus const status)
{
    KINETIC_ASSERT(operation != NULL);
    KINETIC_ASSERT(operation->connection != NULL);
    LOGF3("SetClusterVersion callback w/ operation (0x%0llX) on connection (0x%0llX)",
        operation, operation->connection);
    if (status == KINETIC_STATUS_SUCCESS) {
        KineticSession_SetClusterVersion(operation->connection->pSession, operation->pendingClusterVersion);
        operation->pendingClusterVersion = -1; // Invalidate
    }
    return status;
}

void KineticOperation_BuildSetClusterVersion(KineticOperation* operation, int64_t new_cluster_version)
{
    KineticOperation_ValidateOperation(operation);
    
    operation->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    operation->request->message.command.header->has_messageType = true;
    operation->request->command->body = &operation->request->message.body;
    
    operation->request->command->body->setup = &operation->request->message.setup;
    operation->request->command->body->setup->newClusterVersion = new_cluster_version;
    operation->request->command->body->setup->has_newClusterVersion = true;

    operation->request->command->body = &operation->request->message.body;

    operation->valueEnabled = false;
    operation->sendValue = false;
    operation->callback = &KineticOperation_SetClusterVersionCallback;
    operation->pendingClusterVersion = new_cluster_version;
}
