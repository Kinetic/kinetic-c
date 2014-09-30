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

#ifndef _KINETIC_TYPES_INTERNAL_H
#define _KINETIC_TYPES_INTERNAL_H

#include "kinetic_types.h"
#include "kinetic_proto.h"
#include <netinet/in.h>
#include <ifaddrs.h>
#include <openssl/sha.h>
#include <time.h>

#define KINETIC_SESSIONS_MAX (6)
#define KINETIC_PDUS_PER_SESSION_DEFAULT (2)
#define KINETIC_PDUS_PER_SESSION_MAX (10)
#define KINETIC_SOCKET_DESCRIPTOR_INVALID (-1)

// Ensure __func__ is defined (for debugging)
#if !defined __func__
#define __func__ __FUNCTION__
#endif

// Expose normally private data for test builds to allow inspection
#ifdef TEST
#define STATIC
#else
#define STATIC static
#endif


// Kinetic generic double-linked list item
typedef struct _KineticListItem KineticListItem;
struct _KineticListItem {
    KineticListItem* next;
    KineticListItem* previous;
    void* data;
};
typedef struct _KineticList {
    KineticListItem* start;
    KineticListItem* last;
} KineticList;

typedef struct _KineticPDU KineticPDU;

// Kinetic Device Client Connection
typedef struct _KineticConnection {
    bool    connected;       // state of connection
    int     socket;          // socket file descriptor
    int64_t connectionID;    // initialized to seconds since epoch
    int64_t sequence;        // increments for each request in a session
    KineticList pdus;        // list of dynamically allocated PDUs
    KineticSession session;  // session configuration
} KineticConnection;
#define KINETIC_CONNECTION_INIT(_con) { \
    (*_con) = (KineticConnection) { \
        .connected = false, \
        .socket = -1, \
        .connectionID = time(NULL), \
        .sequence = 0, \
    }; \
    /*(*_con).key = (ByteArray){.data = (*_con).keyData, .len = (_key).len};*/ \
    /*if ((_key).data != NULL && (_key).len > 0) {*/ \
    /*    memcpy((_con)->keyData, (_key).data, (_key).len); }*/ \
}


// Kinetic Message HMAC
typedef struct _KineticHMAC {
    KineticProto_Security_ACL_HMACAlgorithm algorithm;
    uint32_t len;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;


// Kinetic Device Message Request
typedef struct _KineticMessage {
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
    assert((_hdr) != NULL); \
    assert((_con) != NULL); \
    *(_hdr) = (KineticProto_Header) { \
        .base = PROTOBUF_C_MESSAGE_INIT(&KineticProto_header__descriptor), \
        .has_clusterVersion = true, \
        .clusterVersion = (_con)->session.clusterVersion, \
        .has_identity = true, \
        .identity = (_con)->session.identity, \
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


// Kinetic PDU Header
#define PDU_HEADER_LEN              (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN           (1024 * 1024)
#define PDU_PROTO_MAX_UNPACKED_LEN  (PDU_PROTO_MAX_LEN * 2)
#define PDU_MAX_LEN                 (PDU_HEADER_LEN + \
                                    PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)
typedef struct __attribute__((__packed__)) _KineticPDUHeader {
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;
#define KINETIC_PDU_HEADER_INIT \
    (KineticPDUHeader) {.versionPrefix = 'F'}


// Kinetic PDU
struct _KineticPDU {
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
    bool protobufDynamicallyExtracted;
    // bool rawProtoEnabled;
    uint8_t protobufRaw[PDU_PROTO_MAX_LEN];

    // Object meta-data to be used/populated if provided and pertinent to the operation
    KineticEntry entry;

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticConnection* connection;
};
#define KINETIC_PDU_INIT(_pdu, _con) { \
    assert((_pdu) != NULL); \
    assert((_con) != NULL); \
    memset(_pdu, 0, sizeof(KineticPDU)); \
    (_pdu)->connection = (_con); \
    (_pdu)->header = KINETIC_PDU_HEADER_INIT; \
    (_pdu)->headerNBO = KINETIC_PDU_HEADER_INIT; \
    /*(_pdu)->value = BYTE_ARRAY_NONE;*/ \
    /*(_pdu)->proto = &(_pdu)->protoData.message.proto;*/ \
    KINETIC_MESSAGE_HEADER_INIT(&((_pdu)->protoData.message.header), (_con)); \
}
#define KINETIC_PDU_INIT_WITH_MESSAGE(_pdu, _con) { \
    KINETIC_PDU_INIT((_pdu), (_con)) \
    (_pdu)->proto = &(_pdu)->protoData.message.proto; \
    KINETIC_MESSAGE_INIT(&((_pdu)->protoData.message)); \
    (_pdu)->proto->command = &(_pdu)->protoData.message.command; \
    (_pdu)->proto->command->header = &(_pdu)->protoData.message.header; \
    KINETIC_MESSAGE_HEADER_INIT(&(_pdu)->protoData.message.header, (_con)); \
}


// Kinetic Operation
typedef struct _KineticOperation {
    KineticConnection* connection;  // Associated KineticSession
    KineticPDU* request;
    KineticPDU* response;
} KineticOperation;
#define KINETIC_OPERATION_INIT(_op, _con) \
    assert((_op) != NULL); \
    assert((_con) != NULL); \
    *(_op) = (KineticOperation) { \
        .connection = (_con), \
    }

// // Structure for defining a custom memory allocator.
// typedef struct
// {
//     void        *(*alloc)(void *allocator_data, size_t size);
//     void        (*free)(void *allocator_data, void *pointer);
//     // Opaque pointer passed to `alloc` and `free` functions
//     void        *allocator_data;
// } ProtobufCAllocator;

KineticProto_Algorithm KineticProto_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm);
KineticAlgorithm KineticAlgorithm_from_KineticProto_Algorithm(
    KineticProto_Algorithm protoAlgorithm);

KineticProto_Synchronization KineticProto_Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode);
KineticSynchronization KineticSynchronization_from_KineticProto_Synchronization(
    KineticProto_Synchronization sync_mode);

KineticStatus KineticProtoStatusCode_to_KineticStatus(
    KineticProto_Status_StatusCode protoStatus);
ByteArray ProtobufCBinaryData_to_ByteArray(ProtobufCBinaryData protoData);
bool Copy_ProtobufCBinaryData_to_ByteBuffer(ByteBuffer dest, ProtobufCBinaryData src);
bool Copy_KineticProto_KeyValue_to_KineticEntry(KineticProto_KeyValue* keyValue, KineticEntry* entry);

#endif // _KINETIC_TYPES_INTERNAL_H
