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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#ifndef _KINETIC_TYPES_H
#define _KINETIC_TYPES_H

// Include C99 bool definition, if not already defined
#if !defined(__bool_true_false_are_defined) || (__bool_true_false_are_defined == 0)
#include <stdbool.h>
#endif
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>

#include <netinet/in.h>
#include <ifaddrs.h>
#include <openssl/sha.h>

#define KINETIC_PORT            8123
#define KINETIC_TLS_PORT        8443
#define KINETIC_HMAC_SHA1_LEN   (SHA_DIGEST_LENGTH)
#define KINETIC_HMAC_MAX_LEN    (KINETIC_HMAC_SHA1_LEN)
#define KINETIC_MAX_KEY_LEN     128

// Ensure __func__ is defined (for debugging)
#if __STDC_VERSION__ < 199901L
    #if __GNUC__ >= 2
        #define __func__ __FUNCTION__
    #else
        #define __func__ "<unknown>"
    #endif
#endif

// Define max host name length
// Some Linux environments require this, although not all, but it's benign.
#ifndef _BSD_SOURCE
    #define _BSD_SOURCE
#endif // _BSD_SOURCE
#include <unistd.h>
#include <sys/types.h>
#ifndef HOST_NAME_MAX
    #define HOST_NAME_MAX 256
#endif // HOST_NAME_MAX

#include "kinetic_proto.h"
#include <time.h>

typedef ProtobufCBinaryData ByteArray;
#define BYTE_ARRAY_NONE \
    (ByteArray){.len = 0, .data = NULL}
#define BYTE_ARRAY_INIT(_data) (ByteArray) \
    {.data = (uint8_t*)(_data), .len = sizeof(_data)};
#define BYTE_ARRAY_INIT_WITH_LEN(_data, _len) \
    (ByteArray){.data = (uint8_t*)(_data), .len = (_len)};
#define BYTE_ARRAY_CREATE(name, len) \
    uint8_t ( name ## _buf )[(len)]; ByteArray (name) = BYTE_ARRAY_INIT(( name ## _buf ));
#define BYTE_ARRAY_CREATE_WITH_DATA(_name, _data) \
    uint8_t ( _name ## _data )[sizeof(_data)]; ByteArray (_name) = {.data = (uint8_t*(_data)), .len = sizeof(data)};
#define BYTE_ARRAY_CREATE_WITH_BUFFER(_name, _buf) \
    ByteArray (_name) = {.data = (uint8_t*(_buf)), .len = 0};
#define BYTE_ARRAY_INIT_FROM_CSTRING(str) \
    (ByteArray){.data = (uint8_t*)(str), .len = strlen(str)}
#define BYTE_ARRAY_FILL_WITH_DUMMY_DATA(_array) \
    {size_t i=0; for(;i<(_array).len;++i){(_array).data[i] = (uint8_t)(i & 0xFFu);} }


// // Structure for defining a custom memory allocator.
// typedef struct 
// {
//     void        *(*alloc)(void *allocator_data, size_t size);
//     void        (*free)(void *allocator_data, void *pointer);
//     // Opaque pointer passed to `alloc` and `free` functions
//     void        *allocator_data;
// } ProtobufCAllocator;

typedef struct
{
    ByteArray   buffer;
    size_t      maxLen;
} ByteBuffer;
#define BYTE_BUFFER_INIT(_buf, _max) (ByteBuffer) { \
    .buffer = {.data = (uint8_t*)(_buf), .len = 0}, \
    .maxLen = sizeof(_buf) }


// Kinetic Device Client Connection
typedef struct _KineticConnection
{
    bool    connected;
    bool    nonBlocking;
    int     port;
    int     socketDescriptor;
    int64_t connectionID;
    char    host[HOST_NAME_MAX];

    // Optional field - default value is 0
    // The version number of this cluster definition. If this is not equal to
    // the value on the device, the request is rejected and will return a
    // `VERSION_FAILURE` `statusCode` in the `Status` message.
    int64_t clusterVersion;

    // Required field
    // The identity associated with this request. See the ACL discussion above.
    // The Kinetic Device will use this identity value to lookup the
    // HMAC key (shared secret) to verify the HMAC.
    int64_t identity;

    // Required field
    // This is the identity's HMAC Key. This is a shared secret between the
    // client and the device, used to sign requests.
    uint8_t keyData[KINETIC_MAX_KEY_LEN];
    ByteArray key;

    // Required field
    // A monotonically increasing number for each request in a TCP connection.
    int64_t sequence;
} KineticConnection;
#define KINETIC_CONNECTION_INIT(_con, _id, _key) { \
    (*_con) = (KineticConnection) { \
        .socketDescriptor = -1, \
        .connectionID = time(NULL), \
        .identity = (_id), \
        .sequence = 0, \
    }; \
    (*_con).key = (ByteArray){.data = (*_con).keyData, .len = (_key).len}; \
    if ((_key).data != NULL && (_key).len > 0) { \
        memcpy((_con)->keyData, (_key).data, (_key).len); } \
}


// Kinetic Message HMAC
typedef struct _KineticHMAC
{
    KineticProto_Security_ACL_HMACAlgorithm algorithm;
    uint32_t len;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;


// Kinetic Device Message Request
typedef struct _KineticMessage
{
    // Kinetic Protocol Buffer Elements
    KineticProto                proto;
    KineticProto_Command        command;
    KineticProto_Header         header;
    KineticProto_Body           body;
    KineticProto_Status         status;
    KineticProto_Security       security;
    KineticProto_Security_ACL   acl;
    KineticProto_KeyValue       keyValue;
    uint8_t                     hmacData[KINETIC_HMAC_MAX_LEN];
} KineticMessage;
#define KINETIC_MESSAGE_HEADER_INIT(_hdr, _con) { \
    assert((void *)(_hdr) != NULL); \
    assert((void *)(_con) != NULL); \
    *(_hdr) = (KineticProto_Header) { \
        .base = PROTOBUF_C_MESSAGE_INIT(&KineticProto_header__descriptor), \
        .has_clusterVersion = true, \
        .clusterVersion = (_con)->clusterVersion, \
        .has_identity = true, \
        .identity = (_con)->identity, \
        .has_connectionID = true, \
        .connectionID = (_con)->connectionID, \
        .has_sequence = true, \
        .sequence = (_con)->sequence, \
    }; \
}
#define KINETIC_MESSAGE_INIT(msg) { \
    KineticProto__init(&(msg)->proto); \
    KineticProto_command__init(&(msg)->command); \
    KineticProto_header__init(&(msg)->header); \
    KineticProto_status__init(&(msg)->status); \
    KineticProto_body__init(&(msg)->body); \
    KineticProto_key_value__init(&(msg)->keyValue); \
    memset((msg)->hmacData, 0, SHA_DIGEST_LENGTH); \
    (msg)->proto.hmac.data = (msg)->hmacData; \
    (msg)->proto.hmac.len = KINETIC_HMAC_MAX_LEN; \
    (msg)->proto.has_hmac = true; \
    (msg)->command.header = &(msg)->header; \
    (msg)->proto.command = &(msg)->command; \
}


// KeyValue meta-data
typedef struct _Kinetic_KeyValue
{
    ByteArray key;
    ByteArray newVersion;
    ByteArray dbVersion;
    ByteArray tag;
    bool force;
    KineticProto_Algorithm algorithm;
    bool metadataOnly;
    KineticProto_Synchronization synchronization;
    ByteArray value;
} Kinetic_KeyValue;


// Kinetic PDU Header
#define PDU_HEADER_LEN              (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN           (1024 * 1024)
#define PDU_PROTO_MAX_UNPACKED_LEN  (PDU_PROTO_MAX_LEN * 2)
#define PDU_VALUE_MAX_LEN           (1024 * 1024)
#define PDU_MAX_LEN                 (PDU_HEADER_LEN + \
                                    PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)
typedef struct __attribute__ ((__packed__)) _KineticPDUHeader
{
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;
#define KINETIC_PDU_HEADER_INIT \
    (KineticPDUHeader) {.versionPrefix = 'F'}


// Kinetic PDU
typedef struct _KineticPDU
{
    // Binary PDU header
    KineticPDUHeader header;    // Header struct in native byte order
    KineticPDUHeader headerNBO; // Header struct in network-byte-order

    // Message associated with this PDU instance
    union {
        KineticProto protoBase;

        // Pre-structured message w/command
        KineticMessage message;

        // Pad protobuf to max size for extraction of arbitrary packed proto
        uint8_t buffer[PDU_PROTO_MAX_UNPACKED_LEN];
    } protoData;        // Proto will always be first
    KineticProto* proto;
    // bool rawProtoEnabled;
    uint8_t protobufRaw[PDU_PROTO_MAX_LEN];

    // Object meta-data to be used/populated if provided and pertinent to the opearation
    Kinetic_KeyValue* metadata;

    // Value data associated with PDU (if any)
    uint8_t valueBuffer[PDU_VALUE_MAX_LEN];
    ByteArray value;

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticConnection* connection;
} KineticPDU;

#define KINETIC_PDU_INIT(_pdu, _con) { \
    assert((void *)(_pdu) != NULL); \
    assert((void *)(_con) != NULL); \
    (_pdu)->connection = (_con); \
    (_pdu)->header = KINETIC_PDU_HEADER_INIT; \
    (_pdu)->headerNBO = KINETIC_PDU_HEADER_INIT; \
    (_pdu)->value = BYTE_ARRAY_NONE; \
    (_pdu)->proto = &(_pdu)->protoData.message.proto; \
    KINETIC_MESSAGE_HEADER_INIT(&((_pdu)->protoData.message.header), (_con)); \
}
#define KINETIC_PDU_INIT_WITH_MESSAGE(_pdu, _con) { \
    KINETIC_PDU_INIT((_pdu), (_con)) \
    KINETIC_MESSAGE_INIT(&((_pdu)->protoData.message)); \
    (_pdu)->proto->command->header = &(_pdu)->protoData.message.header; \
    KINETIC_MESSAGE_HEADER_INIT(&(_pdu)->protoData.message.header, (_con)); \
}

// Kinetic Operation
typedef struct _KineticOperation
{
    KineticConnection* connection;
    KineticPDU* request;
    KineticPDU* response;
} KineticOperation;
#define KINETIC_OPERATION_INIT(_op, _con, _req, _resp) \
*(_op) = (KineticOperation) { \
    .connection = (_con), \
    .request = (_req), \
    .response = (_resp), \
}


#endif // _KINETIC_TYPES_H
