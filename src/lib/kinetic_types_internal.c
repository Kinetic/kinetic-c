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

#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "byte_array.h"
#include <sys/param.h>
#include <errno.h>

// Type mapping from from public to internal protobuf status type
KineticStatus KineticProtoStatusCode_to_KineticStatus(
    Com_Seagate_Kinetic_Proto_Command_Status_StatusCode protoStatus)
{
    KineticStatus status;

    switch (protoStatus) {

    // Start one-to-one status mappings
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS:
        status = KINETIC_STATUS_SUCCESS;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR:
        status = KINETIC_STATUS_CONNECTION_ERROR;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SERVICE_BUSY:
        status = KINETIC_STATUS_DEVICE_BUSY;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_VERSION_FAILURE:
        status = KINETIC_STATUS_CLUSTER_MISMATCH;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_VERSION_MISMATCH:
        status = KINETIC_STATUS_VERSION_MISMATCH;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_FOUND:
        status = KINETIC_STATUS_NOT_FOUND;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_HMAC_FAILURE:
        status = KINETIC_STATUS_HMAC_FAILURE;
        break;
    // End one-to-one mappings

    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_REQUEST:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_ATTEMPTED:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_HEADER_REQUIRED:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM:
        status = KINETIC_STATUS_INVALID_REQUEST;
        break;

    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DATA_ERROR:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR:
        status = KINETIC_STATUS_DATA_ERROR;
        break;

    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INTERNAL_ERROR:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_EXPIRED:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS:
        status = KINETIC_STATUS_OPERATION_FAILED;
        break;

    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_AUTHORIZED:
        status = KINETIC_STATUS_NOT_AUTHORIZED;
        break;

    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DEVICE_LOCKED:
        status = KINETIC_STATUS_DEVICE_LOCKED;
        break;

    default:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_STATUS_CODE:
        status = KINETIC_STATUS_INVALID;
        break;
    }

    return status;
}

Com_Seagate_Kinetic_Proto_Command_Synchronization Com_Seagate_Kinetic_Proto_Command_Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode)
{
    Com_Seagate_Kinetic_Proto_Command_Synchronization protoSyncMode;
    switch (sync_mode) {
    case KINETIC_SYNCHRONIZATION_WRITETHROUGH:
        protoSyncMode = COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITETHROUGH;
        break;
    case KINETIC_SYNCHRONIZATION_WRITEBACK:
        protoSyncMode = COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITEBACK;
        break;
    case KINETIC_SYNCHRONIZATION_FLUSH:
        protoSyncMode = COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_FLUSH;
        break;
    default:
    case KINETIC_SYNCHRONIZATION_INVALID:
        protoSyncMode = COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_INVALID_SYNCHRONIZATION;
        break;
    };
    return protoSyncMode;
}

KineticSynchronization KineticSynchronization_from_Com_Seagate_Kinetic_Proto_Command_Synchronization(
    Com_Seagate_Kinetic_Proto_Command_Synchronization sync_mode)
{
    KineticSynchronization kineticSyncMode;
    switch (sync_mode) {
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITETHROUGH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITETHROUGH;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITEBACK:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITEBACK;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_FLUSH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_FLUSH;
        break;
    default:
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_SYNCHRONIZATION_INVALID_SYNCHRONIZATION:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_INVALID;
        break;
    };
    return kineticSyncMode;
}

// Type mapping from public to internal types
Com_Seagate_Kinetic_Proto_Command_Algorithm Com_Seagate_Kinetic_Proto_Command_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm)
{
    Com_Seagate_Kinetic_Proto_Command_Algorithm protoAlgorithm;
    switch (kinteicAlgorithm) {
    case KINETIC_ALGORITHM_SHA1:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA1;
        break;
    case KINETIC_ALGORITHM_SHA2:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA2;
        break;
    case KINETIC_ALGORITHM_SHA3:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA3;
        break;
    case KINETIC_ALGORITHM_CRC32:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_CRC32;
        break;
    case KINETIC_ALGORITHM_CRC64:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_CRC64;
        break;
    case KINETIC_ALGORITHM_INVALID:
    default:
        protoAlgorithm = COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_INVALID_ALGORITHM;
        break;
    };
    return protoAlgorithm;
}



// Type mapping from internal types
KineticAlgorithm KineticAlgorithm_from_Com_Seagate_Kinetic_Proto_Command_Algorithm(
    Com_Seagate_Kinetic_Proto_Command_Algorithm protoAlgorithm)
{
    KineticAlgorithm kineticAlgorithm;
    switch (protoAlgorithm) {
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA1:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA1;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA2:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA2;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_SHA3:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA3;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_CRC32:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC32;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_CRC64:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC64;
        break;
    case COM_SEAGATE_KINETIC_PROTO_COMMAND_ALGORITHM_INVALID_ALGORITHM:
    default:
        kineticAlgorithm = KINETIC_ALGORITHM_INVALID;
        break;
    };
    return kineticAlgorithm;
}

ByteArray ProtobufCBinaryData_to_ByteArray(
    ProtobufCBinaryData protoData)
{
    return (ByteArray) {
        .data = protoData.data, .len = protoData.len
    };
}


bool Copy_ProtobufCBinaryData_to_ByteBuffer(ByteBuffer dest, ProtobufCBinaryData src)
{
    if (src.data == NULL || src.len == 0) {
        return false;
    }
    if (dest.array.data == NULL || dest.array.len < src.len) {
        return false;
    }

    bool success = (memcpy(dest.array.data, src.data, src.len) == dest.array.data);
    if (success) {
        dest.bytesUsed = src.len;
    }

    return success;
}


bool Copy_Com_Seagate_Kinetic_Proto_Command_KeyValue_to_KineticEntry(Com_Seagate_Kinetic_Proto_Command_KeyValue* keyValue, KineticEntry* entry)
{
    bool bufferOverflow = false;

    if (keyValue != NULL && entry != NULL) {
        ByteBuffer_Reset(&entry->dbVersion);
        if (keyValue->has_dbVersion && keyValue->dbVersion.len > 0) {
            if (entry->dbVersion.array.data == NULL || entry->dbVersion.array.len < keyValue->dbVersion.len) {
                entry->dbVersion.bytesUsed = keyValue->dbVersion.len;
                LOG1(" BUFFER_OVERRUN: dbVersion");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->dbVersion, keyValue->dbVersion.data, keyValue->dbVersion.len);
            }
        }

        ByteBuffer_Reset(&entry->key);
        if (keyValue->has_key && keyValue->key.len > 0) {
            if (entry->key.array.data == NULL || entry->key.array.len < keyValue->key.len) {
                entry->key.bytesUsed = keyValue->key.len;
                LOG1(" BUFFER_OVERRUN: key");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->key, keyValue->key.data, keyValue->key.len);
            }
        }

        ByteBuffer_Reset(&entry->tag);
        if (keyValue->has_tag && keyValue->tag.len > 0) {
            if (entry->tag.array.data == NULL || entry->tag.array.len < keyValue->tag.len) {
                entry->tag.bytesUsed = keyValue->tag.len;
                LOG1(" BUFFER_OVERRUN: tag");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->tag, keyValue->tag.data, keyValue->tag.len);
            }
        }

        if (keyValue->has_algorithm) {
            entry->algorithm =
                KineticAlgorithm_from_Com_Seagate_Kinetic_Proto_Command_Algorithm(
                    keyValue->algorithm);
        }
    }

    return !bufferOverflow;
}

bool Copy_Com_Seagate_Kinetic_Proto_Command_Range_to_ByteBufferArray(Com_Seagate_Kinetic_Proto_Command_Range* keyRange, ByteBufferArray* keys)
{
    bool bufferOverflow = false;
    LOGF2("Copying: keyRange=0x%0llX, keys=0x%0llX, max_keys=%lld", keyRange, keys->buffers, keys->count);
    if (keyRange != NULL && keys->count > 0 && keys != NULL) {
        for (size_t i = 0; i < MIN((size_t)keys->count, (size_t)keyRange->n_keys); i++) {
            ByteBuffer_Reset(&keys->buffers[i]);
            if (ByteBuffer_Append(&keys->buffers[i], keyRange->keys[i].data, keyRange->keys[i].len) == NULL) {
                LOGF2("WANRNING: Buffer overrun for keys[%zd]", i);
                bufferOverflow = true;
            }
        }
        keys->used = keyRange->n_keys;
    }
    return !bufferOverflow;
}

int Kinetic_GetErrnoDescription(int err_num, char *buf, size_t len)
{
    static pthread_mutex_t strerror_lock = PTHREAD_MUTEX_INITIALIZER;
    if (!len) {
        errno = ENOSPC;
        return -1;
    }
    buf[0] = '\0';
    pthread_mutex_lock(&strerror_lock);
    strncat(buf, strerror(err_num), len - 1);
    pthread_mutex_unlock(&strerror_lock);
    return 0;
}

struct timeval Kinetic_TimevalZero(void)
{
    return (struct timeval) {
        .tv_sec = 0,
        .tv_usec = 0,
    };
}

bool Kinetic_TimevalIsZero(struct timeval const tv)
{
    return tv.tv_sec == 0 && tv.tv_usec == 0;
}

struct timeval Kinetic_TimevalAdd(struct timeval const a, struct timeval const b)
{
    struct timeval result = {
        .tv_sec = a.tv_sec + b.tv_sec,
        .tv_usec = a.tv_usec + b.tv_usec,
    };

    if (result.tv_usec >= 1000000) {
        result.tv_sec++;
        result.tv_usec -= 1000000;
    }
    return result;
}

static int cmp_suseconds_t(suseconds_t const a, suseconds_t const b)
{
    if (a == b) {
        return 0;
    }
    else if (a > b) {
        return 1;
    }
    else {
        return -1;
    }
}

int Kinetic_TimevalCmp(struct timeval const a, struct timeval const b)
{
    return (a.tv_sec == b.tv_sec) ? cmp_suseconds_t(a.tv_usec, b.tv_usec) : ((a.tv_sec > b.tv_sec) ? 1 : -1);
}

Com_Seagate_Kinetic_Proto_Command_GetLog_Type KineticLogInfo_Type_to_Com_Seagate_Kinetic_Proto_Command_GetLog_Type(KineticLogInfo_Type type)
{
    Com_Seagate_Kinetic_Proto_Command_GetLog_Type protoType;

    switch(type) {
    case KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_UTILIZATIONS; break;
    case KINETIC_DEVICE_INFO_TYPE_TEMPERATURES:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_TEMPERATURES; break;
    case KINETIC_DEVICE_INFO_TYPE_CAPACITIES:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_CAPACITIES; break;
    case KINETIC_DEVICE_INFO_TYPE_CONFIGURATION:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_CONFIGURATION; break;
    case KINETIC_DEVICE_INFO_TYPE_STATISTICS:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_STATISTICS; break;
    case KINETIC_DEVICE_INFO_TYPE_MESSAGES:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_MESSAGES; break;
    case KINETIC_DEVICE_INFO_TYPE_LIMITS:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_LIMITS; break;
    default:
        protoType = COM_SEAGATE_KINETIC_PROTO_COMMAND_GET_LOG_TYPE_INVALID_TYPE;
    };

    return protoType;
}

KineticMessageType Com_Seagate_Kinetic_Proto_Command_MessageType_to_KineticMessageType(Com_Seagate_Kinetic_Proto_Command_MessageType type)
{
    return (KineticMessageType)type;
}

void KineticSessionConfig_Copy(KineticSessionConfig* dest, KineticSessionConfig* src)
{
    KINETIC_ASSERT(dest != NULL);
    KINETIC_ASSERT(src != NULL);
    if (dest == src) {return;}
    *dest = *src;
    if (src->hmacKey.data != NULL) {
        memcpy(dest->keyData, src->hmacKey.data, src->hmacKey.len);
    }
}

void KineticMessage_Init(KineticMessage* const message)
{
    KINETIC_ASSERT(message != NULL);

    com_seagate_kinetic_proto_message__init(&message->message);
    com_seagate_kinetic_proto_command__init(&message->command);
    com_seagate_kinetic_proto_message_hmacauth__init(&message->hmacAuth);
    com_seagate_kinetic_proto_message_pinauth__init(&message->pinAuth);
    com_seagate_kinetic_proto_command_header__init(&message->header);
    com_seagate_kinetic_proto_command_status__init(&message->status);
    com_seagate_kinetic_proto_command_body__init(&message->body);
    com_seagate_kinetic_proto_command_key_value__init(&message->keyValue);
    com_seagate_kinetic_proto_command_range__init(&message->keyRange);
    com_seagate_kinetic_proto_command_setup__init(&message->setup);
    com_seagate_kinetic_proto_command_get_log__init(&message->getLog);
    com_seagate_kinetic_proto_command_get_log_device__init(&message->getLogDevice);
    com_seagate_kinetic_proto_command_security__init(&message->security);
    com_seagate_kinetic_proto_command_pin_operation__init(&message->pinOp);
}

static void KineticMessage_HeaderInit(Com_Seagate_Kinetic_Proto_Command_Header* hdr, KineticSession const * const session)
{
    KINETIC_ASSERT(hdr != NULL);
    KINETIC_ASSERT(session != NULL);
    *hdr = (Com_Seagate_Kinetic_Proto_Command_Header) {
        .base = PROTOBUF_C_MESSAGE_INIT(&com_seagate_kinetic_proto_command_header__descriptor),
        .has_clusterVersion = true,
        .clusterVersion = session->config.clusterVersion,
        .has_connectionID = true,
        .connectionID = session->connectionID,
        .has_sequence = true,
        .sequence = KINETIC_SEQUENCE_NOT_YET_BOUND,
    };
}

void KineticRequest_Init(KineticRequest* request, KineticSession const * const session)
{
    KINETIC_ASSERT(request != NULL);
    KINETIC_ASSERT(session != NULL);
    memset(request, 0, sizeof(KineticRequest));
    KineticMessage_Init(&(request->message));
    KineticMessage_HeaderInit(&(request->message.header), session);
    request->command = &request->message.command;
    request->command->header = &request->message.header;
}
