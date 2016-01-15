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
#include "kinetic_types_internal.h"

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
    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__NOOP;
    op->request->message.command.header->has_messagetype = true;
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

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PUT;
    op->request->message.command.header->has_messagetype = true;
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
                              Com__Seagate__Kinetic__Proto__Command__MessageType command_id)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = command_id;
    op->request->message.command.header->has_messagetype = true;
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
        COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GET);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildGetPrevious(KineticOperation* const op,
                                   KineticEntry* const entry)
{
    build_get_command(op, entry, &KineticCallbacks_Get,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETPREVIOUS);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildGetNext(KineticOperation* const op,
                                   KineticEntry* const entry)
{
    build_get_command(op, entry, &KineticCallbacks_Get,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETNEXT);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildFlush(KineticOperation* const op)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype =
        COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__FLUSHALLDATA;
    op->request->message.command.header->has_messagetype = true;
    op->opCallback = &KineticCallbacks_Basic;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildDelete(KineticOperation* const op,
                                  KineticEntry* const entry)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__DELETE;
    op->request->message.command.header->has_messagetype = true;
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

    op->request->command->header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETKEYRANGE;
    op->request->command->header->has_messagetype = true;

    KineticMessage_ConfigureKeyRange(&op->request->message, range);

    op->buffers = buffers;
    op->opCallback = &KineticCallbacks_GetKeyRange;

    return KINETIC_STATUS_SUCCESS;
}

Com__Seagate__Kinetic__Proto__Command__P2POperation* build_p2pOp(uint32_t nestingLevel, KineticP2P_Operation const * const p2pOp)
{
    // limit nesting level to KINETIC_P2P_MAX_NESTING
    if (nestingLevel >= KINETIC_P2P_MAX_NESTING) {
        LOGF0("P2P op nesting level is too deep. Max is %d.", KINETIC_P2P_MAX_NESTING);
        return NULL;
    }

    Com__Seagate__Kinetic__Proto__Command__P2POperation* proto_p2pOp = calloc(1, sizeof(Com__Seagate__Kinetic__Proto__Command__P2POperation));
    if (proto_p2pOp == NULL) { goto error_cleanup; }

    com__seagate__kinetic__proto__command__p2_poperation__init(proto_p2pOp);

    proto_p2pOp->peer = calloc(1, sizeof(Com__Seagate__Kinetic__Proto__Command__P2POperation__Peer));
    if (proto_p2pOp->peer == NULL) { goto error_cleanup; }

    com__seagate__kinetic__proto__command__p2_poperation__peer__init(proto_p2pOp->peer);

    proto_p2pOp->peer->hostname = p2pOp->peer.hostname;
    proto_p2pOp->peer->has_port = true;
    proto_p2pOp->peer->port = p2pOp->peer.port;
    proto_p2pOp->peer->has_tls = true;
    proto_p2pOp->peer->tls = p2pOp->peer.tls;

    proto_p2pOp->n_operation = p2pOp->numOperations;
    proto_p2pOp->operation = calloc(p2pOp->numOperations, sizeof(Com__Seagate__Kinetic__Proto__Command__P2POperation__Operation*));
    if (proto_p2pOp->operation == NULL) { goto error_cleanup; }

    for(size_t i = 0; i < proto_p2pOp->n_operation; i++) {
        KINETIC_ASSERT(!ByteBuffer_IsNull(p2pOp->operations[i].key)); // TODO return invalid operand?

        Com__Seagate__Kinetic__Proto__Command__P2POperation__Operation * p2p_op_op = calloc(1, sizeof(Com__Seagate__Kinetic__Proto__Command__P2POperation__Operation));
        if (p2p_op_op == NULL) { goto error_cleanup; }

        com__seagate__kinetic__proto__command__p2_poperation__operation__init(p2p_op_op);

        p2p_op_op->has_key = true;
        p2p_op_op->key.data = p2pOp->operations[i].key.array.data;
        p2p_op_op->key.len = p2pOp->operations[i].key.bytesUsed;

        p2p_op_op->has_newkey = !ByteBuffer_IsNull(p2pOp->operations[i].newKey);
        p2p_op_op->newkey.data = p2pOp->operations[i].newKey.array.data;
        p2p_op_op->newkey.len = p2pOp->operations[i].newKey.bytesUsed;

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

    op->request->command->header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PEER2PEERPUSH;
    op->request->command->header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->p2pOp = p2pOp;
    op->opCallback = &KineticCallbacks_P2POperation;

    op->request->command->body->p2poperation = build_p2pOp(0, p2pOp);

    if (op->request->command->body->p2poperation == NULL) {
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
    Com__Seagate__Kinetic__Proto__Command__GetLog__Type type, ByteArray name, KineticLogInfo** info)
{
    KineticOperation_ValidateOperation(op);

    op->request->command->header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETLOG;
    op->request->command->header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->getlog = &op->request->message.getLog;
    op->request->command->body->getlog->types = &op->request->message.getLogType;
    op->request->command->body->getlog->types[0] = type;
    op->request->command->body->getlog->n_types = 1;

    if (type == COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE) {
        if (name.data == NULL || name.len == 0) {
            return KINETIC_STATUS_DEVICE_NAME_REQUIRED;
        }
        op->request->message.getLogDevice.name.data = name.data;
        op->request->message.getLogDevice.name.len = name.len;
        op->request->message.getLogDevice.has_name = true;
        op->request->command->body->getlog->device = &op->request->message.getLogDevice;
    }

    op->deviceInfo = info;
    op->opCallback = &KineticCallbacks_GetLog;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetPin(KineticOperation* const op, ByteArray old_pin, ByteArray new_pin, bool lock)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SECURITY;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->security = &op->request->message.security;

    if (lock) {
        op->request->message.security.oldlockpin = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        op->request->message.security.has_oldlockpin = true;
        op->request->message.security.newlockpin = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        op->request->message.security.has_newlockpin = true;
    }
    else {
        op->request->message.security.olderasepin = (ProtobufCBinaryData) {
            .data = old_pin.data, .len = old_pin.len };
        op->request->message.security.has_olderasepin = true;
        op->request->message.security.newerasepin = (ProtobufCBinaryData) {
            .data = new_pin.data, .len = new_pin.len };
        op->request->message.security.has_newerasepin = true;
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
    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->pinop = &op->request->message.pinOp;
    op->request->command->body->pinop->pinoptype = secure_erase ?
        COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__SECURE_ERASE_PINOP :
        COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__ERASE_PINOP;
    op->request->command->body->pinop->has_pinoptype = true;

    op->opCallback = &KineticCallbacks_Basic;
    op->request->pinAuth = true;
    op->timeoutSeconds = KineticOperation_TimeoutErase;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildLockUnlock(KineticOperation* const op, bool lock, ByteArray* pin)
{
    KineticOperation_ValidateOperation(op);

    op->pin = pin;
    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->pinop = &op->request->message.pinOp;

    op->request->command->body->pinop->pinoptype = lock ?
        COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__LOCK_PINOP :
        COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__UNLOCK_PINOP;
    op->request->command->body->pinop->has_pinoptype = true;

    op->opCallback = &KineticCallbacks_Basic;
    op->request->pinAuth = true;
    op->timeoutSeconds = KineticOperation_TimeoutLockUnlock;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetClusterVersion(KineticOperation* op, int64_t new_cluster_version)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SETUP;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;

    op->request->command->body->setup = &op->request->message.setup;
    op->request->command->body->setup->newclusterversion = new_cluster_version;
    op->request->command->body->setup->has_newclusterversion = true;

    op->opCallback = &KineticCallbacks_SetClusterVersion;
    op->pendingClusterVersion = new_cluster_version;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildSetACL(KineticOperation* const op,
    struct ACL *ACLs)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SECURITY;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->security = &op->request->message.security;

    op->request->command->body->security->n_acl = ACLs->ACL_count;
    op->request->command->body->security->acl = ACLs->ACLs;

    op->opCallback = &KineticCallbacks_SetACL;
    op->timeoutSeconds = KineticOperation_TimeoutSetACL;

    return KINETIC_STATUS_SUCCESS;
}

static void initRangeAndPriority(KineticOperation* const op,
		const struct KineticMedia_Operation* media_operation, KineticCommand_Priority priority)
{
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;
    op->request->command->body->security = &op->request->message.security;
    op->request->command->body->range = &op->request->message.keyRange;

    char *data = media_operation->start_key == NULL ? "" : media_operation->start_key;
    op->request->command->body->range->has_startkey = true;
    op->request->command->body->range->startkey = (ProtobufCBinaryData) {
        .data = (uint8_t *)data,
        .len = strlen(data),
    };

    data = media_operation->end_key == NULL ? "" : media_operation->end_key;
    op->request->command->body->range->has_endkey = true;
    op->request->command->body->range->endkey = (ProtobufCBinaryData) {
        .data = (uint8_t *)data,
        .len = strlen(data),
    };

    op->request->command->body->range->startkeyinclusive = media_operation->start_key_inclusive;
    op->request->command->body->range->endkeyinclusive = media_operation->end_key_inclusive;
    op->request->command->body->range->has_startkeyinclusive = true;
    op->request->command->body->range->has_endkeyinclusive = true;

    switch(priority)
    {
    case PRIORITY_NORMAL:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__NORMAL;
    	break;
    case PRIORITY_LOWEST:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__LOWEST;
    	break;
    case PRIORITY_LOWER:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__LOWER;
    	break;
    case PRIORITY_HIGHER:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__HIGHER;
    	break;
    case PRIORITY_HIGHEST:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__HIGHEST;
    	break;
    default:
    	op->request->command->header->priority = COM__SEAGATE__KINETIC__PROTO__COMMAND__PRIORITY__NORMAL;
    	break;
    }
}

KineticStatus KineticBuilder_BuildMediaScan(KineticOperation* const op,
	const KineticMediaScan_Operation* mediascan_operation, KineticCommand_Priority priority)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__MEDIASCAN;
    initRangeAndPriority(op, mediascan_operation, priority);
    op->timeoutSeconds = KineticOperation_TimeoutMediaScan;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticBuilder_BuildMediaOptimize(KineticOperation* const op,
	const KineticMediaOptimize_Operation* mediaoptimize_operation, KineticCommand_Priority priority)
{
    KineticOperation_ValidateOperation(op);

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__MEDIAOPTIMIZE;
    initRangeAndPriority(op, mediaoptimize_operation, priority);
    op->timeoutSeconds = KineticOperation_TimeoutMediaOptimize;

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

    size_t read_size = fread(op->value.data, 1, len, fp);
    if ((long)read_size != len) {
        LOGF0("ERROR: Expected to read %ld bytes from FW file, but read %zu", len, read_size);
        status = KINETIC_STATUS_INVALID_FILE;
        goto cleanup;
    }
    fclose(fp);

    op->value.len = len;

    op->request->message.command.header->messagetype = COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SETUP;
    op->request->message.command.header->has_messagetype = true;
    op->request->command->body = &op->request->message.body;

    op->request->command->body->setup = &op->request->message.setup;
    op->request->command->body->setup->firmwaredownload = true;
    op->request->command->body->setup->has_firmwaredownload = true;

    op->opCallback = &KineticCallbacks_UpdateFirmware;

    return KINETIC_STATUS_SUCCESS;

cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    return status;
}
