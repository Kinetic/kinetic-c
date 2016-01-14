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

#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "byte_array.h"
#include <sys/param.h>
#include <errno.h>

// Type mapping from from public to internal protobuf status type
KineticStatus KineticProtoStatusCode_to_KineticStatus(
    Com__Seagate__Kinetic__Proto__Command__Status__StatusCode protoStatus)
{
    KineticStatus status;

    switch (protoStatus) {

    // Start one-to-one status mappings
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__SUCCESS:
        status = KINETIC_STATUS_SUCCESS;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__REMOTE_CONNECTION_ERROR:
        status = KINETIC_STATUS_CONNECTION_ERROR;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__SERVICE_BUSY:
        status = KINETIC_STATUS_DEVICE_BUSY;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__VERSION_FAILURE:
        status = KINETIC_STATUS_CLUSTER_MISMATCH;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__VERSION_MISMATCH:
        status = KINETIC_STATUS_VERSION_MISMATCH;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_FOUND:
        status = KINETIC_STATUS_NOT_FOUND;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__HMAC_FAILURE:
        status = KINETIC_STATUS_HMAC_FAILURE;
        break;
    // End one-to-one mappings

    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INVALID_REQUEST:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_ATTEMPTED:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__HEADER_REQUIRED:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SUCH_HMAC_ALGORITHM:
        status = KINETIC_STATUS_INVALID_REQUEST;
        break;

    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__DATA_ERROR:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__PERM_DATA_ERROR:
        status = KINETIC_STATUS_DATA_ERROR;
        break;

    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INTERNAL_ERROR:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__EXPIRED:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NESTED_OPERATION_ERRORS:
        status = KINETIC_STATUS_OPERATION_FAILED;
        break;

    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_AUTHORIZED:
        status = KINETIC_STATUS_NOT_AUTHORIZED;
        break;

    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__DEVICE_LOCKED:
        status = KINETIC_STATUS_DEVICE_LOCKED;
        break;

    default:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INVALID_STATUS_CODE:
        status = KINETIC_STATUS_INVALID;
        break;
    }

    return status;
}

Com__Seagate__Kinetic__Proto__Command__Synchronization Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode)
{
    Com__Seagate__Kinetic__Proto__Command__Synchronization protoSyncMode;
    switch (sync_mode) {
    case KINETIC_SYNCHRONIZATION_WRITETHROUGH:
        protoSyncMode = COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITETHROUGH;
        break;
    case KINETIC_SYNCHRONIZATION_WRITEBACK:
        protoSyncMode = COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITEBACK;
        break;
    case KINETIC_SYNCHRONIZATION_FLUSH:
        protoSyncMode = COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__FLUSH;
        break;
    default:
    case KINETIC_SYNCHRONIZATION_INVALID:
        protoSyncMode = COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__INVALID_SYNCHRONIZATION;
        break;
    };
    return protoSyncMode;
}

KineticSynchronization KineticSynchronization_from_Com__Seagate__Kinetic__Proto__Command__Synchronization(
    Com__Seagate__Kinetic__Proto__Command__Synchronization sync_mode)
{
    KineticSynchronization kineticSyncMode;
    switch (sync_mode) {
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITETHROUGH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITETHROUGH;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITEBACK:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITEBACK;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__FLUSH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_FLUSH;
        break;
    default:
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__INVALID_SYNCHRONIZATION:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_INVALID;
        break;
    };
    return kineticSyncMode;
}

// Type mapping from public to internal types
Com__Seagate__Kinetic__Proto__Command__Algorithm Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm)
{
    Com__Seagate__Kinetic__Proto__Command__Algorithm protoAlgorithm;
    switch (kinteicAlgorithm) {
    case KINETIC_ALGORITHM_SHA1:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA1;
        break;
    case KINETIC_ALGORITHM_SHA2:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA2;
        break;
    case KINETIC_ALGORITHM_SHA3:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA3;
        break;
    case KINETIC_ALGORITHM_CRC32:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC32;
        break;
    case KINETIC_ALGORITHM_CRC64:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC64;
        break;
    case KINETIC_ALGORITHM_INVALID:
    default:
        protoAlgorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__INVALID_ALGORITHM;
        break;
    };
    return protoAlgorithm;
}



// Type mapping from internal types
KineticAlgorithm KineticAlgorithm_from_Com__Seagate__Kinetic__Proto__Command__Algorithm(
    Com__Seagate__Kinetic__Proto__Command__Algorithm protoAlgorithm)
{
    KineticAlgorithm kineticAlgorithm;
    switch (protoAlgorithm) {
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA1:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA1;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA2:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA2;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA3:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA3;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC32:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC32;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC64:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC64;
        break;
    case COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__INVALID_ALGORITHM:
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


bool Copy_Com__Seagate__Kinetic__Proto__Command__KeyValue_to_KineticEntry(Com__Seagate__Kinetic__Proto__Command__KeyValue* key_value, KineticEntry* entry)
{
    bool bufferOverflow = false;

    if (key_value != NULL && entry != NULL) {
        ByteBuffer_Reset(&entry->dbVersion);
        if (key_value->has_dbversion && key_value->dbversion.len > 0) {
            if (entry->dbVersion.array.data == NULL || entry->dbVersion.array.len < key_value->dbversion.len) {
                entry->dbVersion.bytesUsed = key_value->dbversion.len;
                LOG1(" BUFFER_OVERRUN: dbVersion");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->dbVersion, key_value->dbversion.data, key_value->dbversion.len);
            }
        }

        ByteBuffer_Reset(&entry->key);
        if (key_value->has_key && key_value->key.len > 0) {
            if (entry->key.array.data == NULL || entry->key.array.len < key_value->key.len) {
                entry->key.bytesUsed = key_value->key.len;
                LOG1(" BUFFER_OVERRUN: key");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->key, key_value->key.data, key_value->key.len);
            }
        }

        ByteBuffer_Reset(&entry->tag);
        if (key_value->has_tag && key_value->tag.len > 0) {
            if (entry->tag.array.data == NULL || entry->tag.array.len < key_value->tag.len) {
                entry->tag.bytesUsed = key_value->tag.len;
                LOG1(" BUFFER_OVERRUN: tag");
                bufferOverflow = true;
            }
            else {
                ByteBuffer_Append(&entry->tag, key_value->tag.data, key_value->tag.len);
            }
        }

        if (key_value->has_algorithm) {
            entry->algorithm =
                KineticAlgorithm_from_Com__Seagate__Kinetic__Proto__Command__Algorithm(
                    key_value->algorithm);
        }
    }

    return !bufferOverflow;
}

bool Copy_Com__Seagate__Kinetic__Proto__Command__Range_to_ByteBufferArray(Com__Seagate__Kinetic__Proto__Command__Range* keyRange, ByteBufferArray* keys)
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

Com__Seagate__Kinetic__Proto__Command__GetLog__Type KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KineticLogInfo_Type type)
{
    Com__Seagate__Kinetic__Proto__Command__GetLog__Type protoType;

    switch(type) {
    case KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__UTILIZATIONS; break;
    case KINETIC_DEVICE_INFO_TYPE_TEMPERATURES:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__TEMPERATURES; break;
    case KINETIC_DEVICE_INFO_TYPE_CAPACITIES:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__CAPACITIES; break;
    case KINETIC_DEVICE_INFO_TYPE_CONFIGURATION:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__CONFIGURATION; break;
    case KINETIC_DEVICE_INFO_TYPE_STATISTICS:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__STATISTICS; break;
    case KINETIC_DEVICE_INFO_TYPE_MESSAGES:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__MESSAGES; break;
    case KINETIC_DEVICE_INFO_TYPE_LIMITS:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__LIMITS; break;
    default:
        protoType = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__INVALID_TYPE;
    };

    return protoType;
}

KineticMessageType Com__Seagate__Kinetic__Proto__Command__MessageType_to_KineticMessageType(Com__Seagate__Kinetic__Proto__Command__MessageType type)
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

    com__seagate__kinetic__proto__message__init(&message->message);
    com__seagate__kinetic__proto__command__init(&message->command);
    com__seagate__kinetic__proto__message__hmacauth__init(&message->hmacAuth);
    com__seagate__kinetic__proto__message__pinauth__init(&message->pinAuth);
    com__seagate__kinetic__proto__command__header__init(&message->header);
    com__seagate__kinetic__proto__command__status__init(&message->status);
    com__seagate__kinetic__proto__command__body__init(&message->body);
    com__seagate__kinetic__proto__command__key_value__init(&message->keyValue);
    com__seagate__kinetic__proto__command__range__init(&message->keyRange);
    com__seagate__kinetic__proto__command__setup__init(&message->setup);
    com__seagate__kinetic__proto__command__get_log__init(&message->getLog);
    com__seagate__kinetic__proto__command__get_log__device__init(&message->getLogDevice);
    com__seagate__kinetic__proto__command__security__init(&message->security);
    com__seagate__kinetic__proto__command__pin_operation__init(&message->pinOp);
}

static void KineticMessage_HeaderInit(Com__Seagate__Kinetic__Proto__Command__Header* hdr, KineticSession const * const session)
{
    KINETIC_ASSERT(hdr != NULL);
    KINETIC_ASSERT(session != NULL);
    *hdr = (Com__Seagate__Kinetic__Proto__Command__Header) {
        .base = PROTOBUF_C_MESSAGE_INIT(&com__seagate__kinetic__proto__command__header__descriptor),
        .has_clusterversion = true,
        .clusterversion = session->config.clusterVersion,
        .has_connectionid = true,
        .connectionid = session->connectionID,
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
