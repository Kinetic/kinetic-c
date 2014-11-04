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
#include <pthread.h>

#define KINETIC_SESSIONS_MAX (20)
#define KINETIC_PDUS_PER_SESSION_DEFAULT (2)
#define KINETIC_PDUS_PER_SESSION_MAX (10)
#define KINETIC_SOCKET_DESCRIPTOR_INVALID (-1)
#define KINETIC_CONNECTION_INITIAL_STATUS_TIMEOUT_SECS (3)
#define KINETIC_PDU_RECEIVE_TIMEOUT_SECS (5)

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


typedef struct _KineticPDU KineticPDU;
typedef struct _KineticOperation KineticOperation;
typedef struct _KineticConnection KineticConnection;


// Kinetic list item
typedef struct _KineticListItem KineticListItem;
struct _KineticListItem {
    KineticListItem* next;
    KineticListItem* previous;
    void* data;
};

// Kinetic list
typedef struct _KineticList {
    KineticListItem* start;
    KineticListItem* last;
    pthread_mutex_t mutex;
    bool locked;
} KineticList;
#define KINETIC_LIST_INITIALIZER (KineticList) { \
    .mutex = PTHREAD_MUTEX_INITIALIZER, .locked = false, .start = NULL, .last = NULL }

// Kinetic Thread Instance
typedef struct _KineticThread {
    bool abortRequested;
    bool fatalError;
    bool paused;
    KineticConnection* connection;
} KineticThread;


// Kinetic Device Client Connection
struct _KineticConnection {
    bool            connected;      // state of connection
    int             socket;         // socket file descriptor
    int64_t         connectionID;   // initialized to seconds since epoch
    int64_t         sequence;       // increments for each request in a session
    KineticList     pdus;           // list of dynamically allocated PDUs
    KineticList     operations;     // list of dynamically allocated operations
    KineticSession  session;        // session configuration
    KineticThread   thread;         // worker thread instance struct
    pthread_t       threadID;       // worker pthread
    bool            threadCreated;  // thread creation status
};
#define KINETIC_CONNECTION_INIT(_con) { (*_con) = (KineticConnection) { \
        .connected = false, \
        .socket = -1, \
        .operations = KINETIC_LIST_INITIALIZER, \
        .pdus = KINETIC_LIST_INITIALIZER, \
    }; \
}


// Kinetic Message HMAC
typedef struct _KineticHMAC {
    KineticProto_Command_Security_ACL_HMACAlgorithm algorithm;
    uint32_t len;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;


// Kinetic Device Message Request
typedef struct _KineticMessage {
    // Kinetic Protocol Buffer Elements
    KineticProto_Message                message;
    KineticProto_Message_HMACauth       hmacAuth;
    KineticProto_Message_PINauth        pinAuth;
    uint8_t                             hmacData[KINETIC_HMAC_MAX_LEN];

    bool                                has_command; // Set to `true` to enable command element
    KineticProto_Command                command;
    KineticProto_Command_Header         header;
    KineticProto_Command_Body           body;
    KineticProto_Command_Status         status;
    KineticProto_Command_Security       security;
    KineticProto_Command_Security_ACL   acl;
    KineticProto_Command_KeyValue       keyValue;
    KineticProto_Command_Range          keyRange;
} KineticMessage;

#define KINETIC_MESSAGE_AUTH_HMAC_INIT(_msg, _identity, _hmac) { \
    assert((_msg) != NULL); \
    (_msg)->message.has_authType = true; \
    (_msg)->message.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH; \
    KineticProto_Message_hmacauth__init(&(_msg)->hmacAuth); \
    (_msg)->message.hmacAuth = &(_msg)->hmacAuth; \
    KineticProto_Message_pinauth__init(&(_msg)->pinAuth); \
    (_msg)->message.pinAuth = NULL; \
    (_msg)->command.header = &(_msg)->header; \
    memset((_msg)->hmacData, 0, KINETIC_HMAC_MAX_LEN); \
    if ((_hmac).len <= KINETIC_HMAC_MAX_LEN \
        && (_hmac).data != NULL && (_hmac).len > 0 \
        && (_msg)->hmacData != NULL) { \
        memcpy((_msg)->hmacData, (_hmac).data, (_hmac).len);} \
    (_msg)->message.hmacAuth->has_identity = true; \
    (_msg)->message.hmacAuth->identity = (_identity); \
    (_msg)->message.hmacAuth->has_hmac = true; \
    (_msg)->message.hmacAuth->hmac = (ProtobufCBinaryData) { \
        .data = (_msg)->hmacData, .len = SHA_DIGEST_LENGTH}; \
}

#define KINETIC_MESSAGE_HEADER_INIT(_hdr, _con) { \
    assert((_hdr) != NULL); \
    assert((_con) != NULL); \
    *(_hdr) = (KineticProto_Command_Header) { \
        .base = PROTOBUF_C_MESSAGE_INIT(&KineticProto_command_header__descriptor), \
        .has_clusterVersion = true, \
        .clusterVersion = (_con)->session.clusterVersion, \
        .has_connectionID = true, \
        .connectionID = (_con)->connectionID, \
        .has_sequence = true, \
        .sequence = (_con)->sequence, \
    }; \
}

#define KINETIC_MESSAGE_INIT(msg) { \
    KineticProto_Message__init(&(msg)->message); \
    KineticProto_command__init(&(msg)->command); \
    KineticProto_Message_hmacauth__init(&(msg)->hmacAuth); \
    KineticProto_Message_pinauth__init(&(msg)->pinAuth); \
    KineticProto_command_header__init(&(msg)->header); \
    KineticProto_command_status__init(&(msg)->status); \
    KineticProto_command_body__init(&(msg)->body); \
    KineticProto_command_key_value__init(&(msg)->keyValue); \
    KineticProto_command_range__init(&(msg)->keyRange); \
    KINETIC_MESSAGE_AUTH_HMAC_INIT(msg, 0, BYTE_ARRAY_NONE); \
    (msg)->has_command = false; \
}

// Kinetic PDU Header
#define PDU_HEADER_LEN              (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN           (1024 * 1024)
#define PDU_PROTO_MAX_UNPACKED_LEN  (PDU_PROTO_MAX_LEN * 2)
#define PDU_MAX_LEN                 (PDU_HEADER_LEN + \
                                    PDU_PROTO_MAX_LEN + KINETIC_OBJ_SIZE)
typedef struct __attribute__((__packed__)) _KineticPDUHeader {
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;
#define KINETIC_PDU_HEADER_INIT \
    (KineticPDUHeader) {.versionPrefix = 'F'}

typedef enum {
    KINETIC_PDU_TYPE_INVALID = 0,
    KINETIC_PDU_TYPE_REQUEST,
    KINETIC_PDU_TYPE_RESPONSE,
    KINETIC_PDU_TYPE_UNSOLICITED
} KineticPDUType;


// Kinetic PDU
struct _KineticPDU {
    // Binary PDU header
    KineticPDUHeader header;    // Header struct in native byte order
    KineticPDUHeader headerNBO; // Header struct in network-byte-order

    // Message associated with this PDU instance
    union {
        KineticProto_Message base;
        KineticMessage message;
    } protoData;        // Proto will always be first
    KineticProto_Message* proto;
    bool protobufDynamicallyExtracted;
    KineticProto_Command* command;

    // // Object meta-data to be used/populated if provided and pertinent to the operation
    // KineticEntry entry;

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticConnection* connection;

    // The type of this PDU (request, response or unsolicited)
    KineticPDUType type;

    // Kinetic operation associated with this PDU, if any
    KineticOperation* operation;

    // // PDU associated with this one (for associating request and response PDUs for operations)
    // KineticPDU* associatedPDU;

    // // Operation complete closure
    // KineticCompletionClosure closure;
};

#define KINETIC_PDU_INIT(_pdu, _con) { \
    assert((_pdu) != NULL); \
    assert((_con) != NULL); \
    memset((_pdu), 0, sizeof(KineticPDU)); \
    (_pdu)->connection = (_con); \
    (_pdu)->header = KINETIC_PDU_HEADER_INIT; \
    (_pdu)->headerNBO = KINETIC_PDU_HEADER_INIT; \
    KINETIC_MESSAGE_INIT(&((_pdu)->protoData.message)); \
    KINETIC_MESSAGE_AUTH_HMAC_INIT( \
            &((_pdu)->protoData.message), (_con)->session.identity, (_con)->session.hmacKey); \
    KINETIC_MESSAGE_HEADER_INIT(&((_pdu)->protoData.message.header), (_con)); \
}

#define KINETIC_PDU_INIT_WITH_COMMAND(_pdu, _con) { \
    KINETIC_PDU_INIT((_pdu), (_con)) \
    (_pdu)->proto = &(_pdu)->protoData.message.message; \
    (_pdu)->protoData.message.has_command = true; \
    (_pdu)->command = &(_pdu)->protoData.message.command; \
    (_pdu)->command->header = &(_pdu)->protoData.message.header; \
    (_pdu)->type = KINETIC_PDU_TYPE_REQUEST; \
}

typedef KineticStatus (*KineticOperationCallback)(KineticOperation* operation);

// Kinetic Operation
struct _KineticOperation {
    KineticConnection* connection;
    KineticPDU* request;
    KineticPDU* response;
    bool entryEnabled;
    bool valueEnabled;
    bool sendValue;
    bool receiveComplete;
    KineticEntry* entry;
    ByteBufferArray* buffers;
    KineticOperationCallback callback;
    KineticCompletionClosure closure;
};
#define KINETIC_OPERATION_INIT(_op, _con) \
    assert((_op) != NULL); \
    assert((_con) != NULL); \
    *(_op) = (KineticOperation) {.connection = (_con)}


KineticProto_Command_Algorithm KineticProto_Command_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm);
KineticAlgorithm KineticAlgorithm_from_KineticProto_Command_Algorithm(
    KineticProto_Command_Algorithm protoAlgorithm);

KineticProto_Command_Synchronization KineticProto_Command_Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode);
KineticSynchronization KineticSynchronization_from_KineticProto_Command_Synchronization(
    KineticProto_Command_Synchronization sync_mode);

KineticStatus KineticProtoStatusCode_to_KineticStatus(
    KineticProto_Command_Status_StatusCode protoStatus);
ByteArray ProtobufCBinaryData_to_ByteArray(
    ProtobufCBinaryData protoData);
bool Copy_ProtobufCBinaryData_to_ByteBuffer(
    ByteBuffer dest, ProtobufCBinaryData src);
bool Copy_KineticProto_Command_KeyValue_to_KineticEntry(
    KineticProto_Command_KeyValue* keyValue, KineticEntry* entry);
bool Copy_KineticProto_Command_Range_to_ByteBufferArray(
    KineticProto_Command_Range* keyRange, ByteBufferArray* keys);
int Kinetic_GetErrnoDescription(int err_num, char *buf, size_t len);

#endif // _KINETIC_TYPES_INTERNAL_H
