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

#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "byte_array.h"
#include <sys/param.h>

// Type mapping from from public to internal protobuf status type
KineticStatus KineticProtoStatusCode_to_KineticStatus(
    KineticProto_Status_StatusCode protoStatus)
{
    KineticStatus status;

    switch (protoStatus) {
    case KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS:
        status = KINETIC_STATUS_SUCCESS;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR:
        status = KINETIC_STATUS_CONNECTION_ERROR;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY:
        status = KINETIC_STATUS_DEVICE_BUSY;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_REQUEST:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED:
    case KINETIC_PROTO_STATUS_STATUS_CODE_HEADER_REQUIRED:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM:
        status = KINETIC_STATUS_INVALID_REQUEST;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH:
    case KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_FAILURE:
        status = KINETIC_STATUS_VERSION_FAILURE;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR:
    case KINETIC_PROTO_STATUS_STATUS_CODE_HMAC_FAILURE:
    case KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_FOUND:
        status = KINETIC_STATUS_DATA_ERROR;
        break;

    case KINETIC_PROTO_STATUS_STATUS_CODE_INTERNAL_ERROR:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_AUTHORIZED:
    case KINETIC_PROTO_STATUS_STATUS_CODE_EXPIRED:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NO_SPACE:
    case KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS:
        status = KINETIC_STATUS_OPERATION_FAILED;
        break;

    default:
    case KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE:
    case _KINETIC_PROTO_STATUS_STATUS_CODE_IS_INT_SIZE:
        status = KINETIC_STATUS_INVALID;
        break;
    }

    return status;
}

KineticProto_Synchronization KineticProto_Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode)
{
    KineticProto_Synchronization protoSyncMode;
    switch (sync_mode) {
    case KINETIC_SYNCHRONIZATION_WRITETHROUGH:
        protoSyncMode = KINETIC_PROTO_SYNCHRONIZATION_WRITETHROUGH;
        break;
    case KINETIC_SYNCHRONIZATION_WRITEBACK:
        protoSyncMode = KINETIC_PROTO_SYNCHRONIZATION_WRITEBACK;
        break;
    case KINETIC_SYNCHRONIZATION_FLUSH:
        protoSyncMode = KINETIC_PROTO_SYNCHRONIZATION_FLUSH;
        break;
    default:
    case KINETIC_SYNCHRONIZATION_INVALID:
        protoSyncMode = KINETIC_PROTO_SYNCHRONIZATION_INVALID_SYNCHRONIZATION;
        break;
    };
    return protoSyncMode;
}

KineticSynchronization KineticSynchronization_from_KineticProto_Synchronization(
    KineticProto_Synchronization sync_mode)
{
    KineticSynchronization kineticSyncMode;
    switch (sync_mode) {
    case KINETIC_PROTO_SYNCHRONIZATION_WRITETHROUGH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITETHROUGH;
        break;
    case KINETIC_PROTO_SYNCHRONIZATION_WRITEBACK:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_WRITEBACK;
        break;
    case KINETIC_PROTO_SYNCHRONIZATION_FLUSH:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_FLUSH;
        break;
    default:
    case KINETIC_PROTO_SYNCHRONIZATION_INVALID_SYNCHRONIZATION:
        kineticSyncMode = KINETIC_SYNCHRONIZATION_INVALID;
        break;
    };
    return kineticSyncMode;
}

// Type mapping from public to internal types
KineticProto_Algorithm KineticProto_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm)
{
    KineticProto_Algorithm protoAlgorithm;
    switch (kinteicAlgorithm) {
    case KINETIC_ALGORITHM_SHA1:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_SHA1;
        break;
    case KINETIC_ALGORITHM_SHA2:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_SHA2;
        break;
    case KINETIC_ALGORITHM_SHA3:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_SHA3;
        break;
    case KINETIC_ALGORITHM_CRC32:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_CRC32;
        break;
    case KINETIC_ALGORITHM_CRC64:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_CRC64;
        break;
    case KINETIC_ALGORITHM_INVALID:
    default:
        protoAlgorithm = KINETIC_PROTO_ALGORITHM_INVALID_ALGORITHM;
        break;
    };
    return protoAlgorithm;
}



// Type mapping from internal types
KineticAlgorithm KineticAlgorithm_from_KineticProto_Algorithm(
    KineticProto_Algorithm protoAlgorithm)
{
    KineticAlgorithm kineticAlgorithm;
    switch (protoAlgorithm) {
    case KINETIC_PROTO_ALGORITHM_SHA1:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA1;
        break;
    case KINETIC_PROTO_ALGORITHM_SHA2:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA2;
        break;
    case KINETIC_PROTO_ALGORITHM_SHA3:
        kineticAlgorithm = KINETIC_ALGORITHM_SHA3;
        break;
    case KINETIC_PROTO_ALGORITHM_CRC32:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC32;
        break;
    case KINETIC_PROTO_ALGORITHM_CRC64:
        kineticAlgorithm = KINETIC_ALGORITHM_CRC64;
        break;
    case KINETIC_PROTO_ALGORITHM_INVALID_ALGORITHM:
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


bool Copy_KineticProto_KeyValue_to_KineticEntry(KineticProto_KeyValue* keyValue, KineticEntry* entry)
{
    bool bufferOverflow = false;

    if (keyValue != NULL && entry != NULL) {

        if (keyValue->has_newVersion) {
            entry->newVersion.bytesUsed = keyValue->newVersion.len;
            if (entry->newVersion.array.data == NULL ||
                entry->newVersion.array.len < keyValue->newVersion.len) {
                LOG(" BUFFER_OVERRUN: newVersion");
                bufferOverflow = true;
            }
            else {
                memcpy(entry->newVersion.array.data, keyValue->newVersion.data,
                       keyValue->newVersion.len);
            }
        }

        if (keyValue->has_dbVersion) {
            entry->dbVersion.bytesUsed = keyValue->dbVersion.len;
            if (entry->dbVersion.array.data == NULL ||
                entry->dbVersion.array.len < keyValue->dbVersion.len) {
                LOG(" BUFFER_OVERRUN: dbVersion");
                bufferOverflow = true;
            }
            else {
                memcpy(entry->dbVersion.array.data, keyValue->dbVersion.data,
                       keyValue->dbVersion.len);
            }
        }

        if (keyValue->has_force) {
            entry->force = keyValue->force;
        }

        if (keyValue->has_key) {
            entry->key.bytesUsed = keyValue->key.len;
            if (entry->key.array.data == NULL ||
                entry->key.array.len < keyValue->key.len) {
                LOG(" BUFFER_OVERRUN: key");
                bufferOverflow = true;
            }
            else {
                memcpy(entry->key.array.data, keyValue->key.data, keyValue->key.len);
            }
        }

        if (keyValue->has_tag) {
            entry->tag.bytesUsed = keyValue->tag.len;
            if (entry->tag.array.data == NULL ||
                entry->tag.array.len < keyValue->tag.len) {
                LOG(" BUFFER_OVERRUN: tag");
                bufferOverflow = true;
            }
            else {
                memcpy(entry->tag.array.data, keyValue->tag.data, keyValue->tag.len);
            }
        }

        if (keyValue->has_algorithm) {
            entry->algorithm =
                KineticAlgorithm_from_KineticProto_Algorithm(
                    keyValue->algorithm);
        }

        if (keyValue->has_metadataOnly) {
            entry->metadataOnly = keyValue->metadataOnly;
        }

        if (keyValue->has_synchronization) {
            entry->synchronization =
                KineticSynchronization_from_KineticProto_Synchronization(
                    keyValue->synchronization);
        }
    }

    return !bufferOverflow;
}


bool Copy_KineticProto_Range_to_buffer_list(
    KineticProto_Range* keyRange,
    ByteBuffer* keys,
    int64_t max_keys)
{
    bool bufferOverflow = false;
    LOGF("HERE: keyRange=0x%0llX, keys=0x%0llX, max_keys=%lld", keyRange, keys, max_keys);
    if (keyRange != NULL && keys != NULL && max_keys > 0) {
        LOG("THERE");
        for (size_t i = 0; i < MIN((size_t)max_keys, (size_t)keyRange->n_key); i++) {
            ByteBuffer_Reset(&keys[i]);
            LOGF("EVERYWHERE key[%llu]: actual data=0x%0llX, actual len=%zu, buff len=%zu, buff data=0x%0llX buff used=%zu",
                i, keyRange->key[i].data, keyRange->key[i].len, keys[i].array.len, keys[i].array.data, keys[i].bytesUsed);
            if (ByteBuffer_Append(&keys[i], keyRange->key[i].data, keyRange->key[i].len) == NULL) {
                LOG(" BUFFER_OVERRUN: keyRange");
                bufferOverflow = true;
            }
        }
    }
    return !bufferOverflow;
}
