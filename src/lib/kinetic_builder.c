/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "kinetic_builder.h"
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
#include "kinetic_callbacks.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "kinetic_acl.h"

/*******************************************************************************
 * Standard Client Operations
*******************************************************************************/

KineticStatus KineticBuilder_BuildNoop(KineticOperation* const op)
{
    KineticOperation_ValidateOperation(op);
    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP;
    op->request->message.command.header->has_messageType = true;
    op->opCallback = &KineticCallbacks_Basic;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildPut(KineticOperation* const op,
                               KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(op);

    if (entry->value.bytesUsed > KINETIC_OBJ_SIZE) {
        LOGF2("Value exceeds maximum size. Packed size is: %d, Max size is: %d", entry->value.bytesUsed, KINETIC_OBJ_SIZE);
        return KINETIC_STATUS_BUFFER_OVERRUN;
    }

    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT;
    op->request->message.command.header->has_messageType = true;
    op->entry = entry;

    KineticMessage_ConfigureKeyValue(&op->request->message, op->entry);

    op->value.data = op->entry->value.array.data;
    op->value.len = op->entry->value.bytesUsed;
    op->opCallback = &KineticCallbacks_Put;

    return KINETIC_STATUS_SUCCESS;
}

static void build_get_command(KineticOperation* const op,
                              KineticEntry* const entry,
                              KineticOperationCallback cb,
                              KineticProto_Command_MessageType command_id)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messageType = command_id;
    op->request->message.command.header->has_messageType = true;
    op->entry = entry;

    KineticMessage_ConfigureKeyValue(&op->request->message, entry);

    if (op->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&op->entry->value);
        op->value.data = op->entry->value.array.data;
        op->value.len = op->entry->value.bytesUsed;
    }

    op->opCallback = cb;
}

KineticStatus KineticBuilder_BuildGet(KineticOperation* const op,
                               KineticEntry* const entry)
{
    build_get_command(op, entry, &KineticCallbacks_Get,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildGetPrevious(KineticOperation* const op,
                                   KineticEntry* const entry)
{
    build_get_command(op, entry, &KineticCallbacks_Get,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETPREVIOUS);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildGetNext(KineticOperation* const op,
                                   KineticEntry* const entry)
{
    build_get_command(op, entry, &KineticCallbacks_Get,
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETNEXT);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildFlush(KineticOperation* const op)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messageType =
        KINETIC_PROTO_COMMAND_MESSAGE_TYPE_FLUSHALLDATA;
    op->request->message.command.header->has_messageType = true;
    op->opCallback = &KineticCallbacks_Basic;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildDelete(KineticOperation* const op,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE;
    op->request->message.command.header->has_messageType = true;
    op->entry = entry;

    KineticMessage_ConfigureKeyValue(&op->request->message, op->entry);

    if (op->entry->value.array.data != NULL) {
        ByteBuffer_Reset(&op->entry->value);
        op->value.data = op->entry->value.array.data;
        op->value.len = op->entry->value.bytesUsed;
    }

    op->opCallback = &KineticCallbacks_Delete;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildGetKeyRange(KineticOperation* const op,
    KineticKeyRange* range, ByteBufferArray* buffers)
{
    KineticOperation_ValidateOperation(op);
    KINETIC_ASSERT(range != NULL);
    KINETIC_ASSERT(buffers != NULL);

    op->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE;
    op->request->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyRange(&op->request->message, range);

    op->buffers = buffers;
    op->opCallback = &KineticCallbacks_GetKeyRange;

    return KINETIC_STATUS_SUCCESS;
}

KineticProto_Command_P2POperation* build_p2pOp(uint32_t nestingLevel, KineticP2P_Operation const * const p2pOp)
{
    // limit nesting level to KINETIC_P2P_MAX_NESTING
    if (nestingLevel >= KINETIC_P2P_MAX_NESTING) {
        LOGF0("P2P op nesting level is too deep. Max is %d.", KINETIC_P2P_MAX_NESTING);
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
    KineticAllocator_FreeP2PProtobuf(proto_p2pOp);
    return NULL;
}

KineticStatus KineticBuilder_BuildP2POperation(KineticOperation* const op,
                                                 KineticP2P_Operation* const p2pOp)
{
    KineticOperation_ValidateOperation(op);
        
    op->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PEER2PEERPUSH;
    op->request->command->header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->p2pOp = p2pOp;
    op->opCallback = &KineticCallbacks_P2POperation;

    op->request->command->body->p2pOperation = build_p2pOp(0, p2pOp);
    
    if (op->request->command->body->p2pOperation == NULL) {
        return KINETIC_STATUS_OPERATION_INVALID;
    }

    if (p2pOp->numOperations >= KINETIC_P2P_OPERATION_LIMIT) {
        return KINETIC_STATUS_BUFFER_OVERRUN;
    }

    return KINETIC_STATUS_SUCCESS;
}



/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/

KineticStatus KineticBuilder_BuildGetLog(KineticOperation* const op,
    KineticProto_Command_GetLog_Type type, ByteArray name, KineticLogInfo** info)
{
    KineticOperation_ValidateOperation(op);
        
    op->request->command->header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETLOG;
    op->request->command->header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->getLog = &op->request->message.getLog;
    op->request->command->body->getLog->types = &op->request->message.getLogType;
    op->request->command->body->getLog->types[0] = type;
    op->request->command->body->getLog->n_types = 1;

    if (type == KINETIC_PROTO_COMMAND_GET_LOG_TYPE_DEVICE) {
        if (name.data == NULL || name.len == 0) {
            return KINETIC_STATUS_DEVICE_NAME_REQUIRED;
        }
        op->request->message.getLogDevice.name.data = name.data;
        op->request->message.getLogDevice.name.len = name.len;
        op->request->message.getLogDevice.has_name = true;
        op->request->command->body->getLog->device = &op->request->message.getLogDevice;
    }

    op->deviceInfo = info;
    op->opCallback = &KineticCallbacks_GetLog;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetPin(KineticOperation* const op, ByteArray old_pin, ByteArray new_pin, bool lock)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SECURITY;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->security = &op->request->message.security;

    if (lock) {
        op->request->message.security.oldLockPIN = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        op->request->message.security.has_oldLockPIN = true;
        op->request->message.security.newLockPIN = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        op->request->message.security.has_newLockPIN = true;
    }
    else {
        op->request->message.security.oldErasePIN = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        op->request->message.security.has_oldErasePIN = true;
        op->request->message.security.newErasePIN = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        op->request->message.security.has_newErasePIN = true;
    }
    
    op->opCallback = &KineticCallbacks_Basic;
    op->request->pinAuth = false;
    op->timeoutSeconds = KineticOperation_TimeoutSetPin;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildErase(KineticOperation* const op, bool secure_erase, ByteArray* pin)
{
    KineticOperation_ValidateOperation(op);

    op->pin = pin;
    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->pinOp = &op->request->message.pinOp;
    op->request->command->body->pinOp->pinOpType = secure_erase ?
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_SECURE_ERASE_PINOP :
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_ERASE_PINOP;
    op->request->command->body->pinOp->has_pinOpType = true;
    
    op->opCallback = &KineticCallbacks_Basic;
    op->request->pinAuth = true;
    op->timeoutSeconds = KineticOperation_TimeoutErase;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildLockUnlock(KineticOperation* const op, bool lock, ByteArray* pin)
{
    KineticOperation_ValidateOperation(op);

    op->pin = pin;
    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->pinOp = &op->request->message.pinOp;
    
    op->request->command->body->pinOp->pinOpType = lock ?
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_LOCK_PINOP :
        KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_UNLOCK_PINOP;
    op->request->command->body->pinOp->has_pinOpType = true;
    
    op->opCallback = &KineticCallbacks_Basic;
    op->request->pinAuth = true;
    op->timeoutSeconds = KineticOperation_TimeoutLockUnlock;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetClusterVersion(KineticOperation* op, int64_t new_cluster_version)
{
    KineticOperation_ValidateOperation(op);
    
    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    
    op->request->command->body->setup = &op->request->message.setup;
    op->request->command->body->setup->newClusterVersion = new_cluster_version;
    op->request->command->body->setup->has_newClusterVersion = true;

    op->opCallback = &KineticCallbacks_SetClusterVersion;
    op->pendingClusterVersion = new_cluster_version;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetACL(KineticOperation* const op,
    struct ACL *ACLs)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SECURITY;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->security = &op->request->message.security;

    op->request->command->body->security->n_acl = ACLs->ACL_count;
    op->request->command->body->security->acl = ACLs->ACLs;

    op->opCallback = &KineticCallbacks_SetACL;
    op->timeoutSeconds = KineticOperation_TimeoutSetACL;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildUpdateFirmware(KineticOperation* const op, const char* fw_path)
{
    KineticOperation_ValidateOperation(op);

    KineticStatus status = KINETIC_STATUS_INVALID;
    FILE* fp = NULL;

    if (fw_path == NULL) {
        LOG0("ERROR: FW update file was NULL");
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }

    fp = fopen(fw_path, "r");
    if (fp == NULL) {
        LOG0("ERROR: Specified FW update file could not be opened");
        return KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        LOG0("ERROR: Specified FW update file could not be seek");
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }

    long len = ftell(fp);
    if (len < 1) {
        LOG0("ERROR: Specified FW update file could not be queried for length");
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        LOG0("ERROR: Specified FW update file could not be seek back to start");
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }

    op->value.data = calloc(len, 1);
    if (op->value.data == NULL) {
        LOG0("ERROR: Failed allocating memory to store FW update image");
        status = KINETIC_STATUS_MEMORY_ERROR;
        goto cleanup;
    }

    size_t read = fread(op->value.data, 1, len, fp);
    if ((long)read != len) {
        LOGF0("ERROR: Expected to read %ld bytes from FW file, but read %zu", len, read);
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }
    fclose(fp);

    op->value.len = len;
    
    op->request->message.command.header->messageType = KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP;
    op->request->message.command.header->has_messageType = true;
    op->request->command->body = &op->request->message.body;
    
    op->request->command->body->setup = &op->request->message.setup;
    op->request->command->body->setup->firmwareDownload = true;
    op->request->command->body->setup->has_firmwareDownload = true;

    op->opCallback = &KineticCallbacks_UpdateFirmware;

    return KINETIC_STATUS_SUCCESS;

cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    return status;
}
