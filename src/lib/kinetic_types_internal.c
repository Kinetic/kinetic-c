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

// Type mapping from public to internal types
KineticProto_Algorithm KineticProto_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm)
{
    KineticProto_Algorithm protoAlgorithm;
    {
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
    }
    return protoAlgorithm;
}

ByteArray ByteArray_from_ProtobufCBinaryData(
    ProtobufCBinaryData protoData)
{
    return (ByteArray) {.data = protoData.data, .len = protoData.len};
}

bool Copy_ProtobufCBinaryData_to_ByteArray(ByteArray dest,
    ProtobufCBinaryData src)
{
    if (src.data == NULL || src.len == 0) {
        return false;
    }
    if (dest.data == NULL || dest.len < src.len) {
        return false;
    }

    bool success = (memcpy(dest.data, src.data, src.len) == dest.data);
    if (success) {
        dest.len = src.len;
    }
    
    return success;
}
