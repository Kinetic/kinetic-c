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

#ifndef _KINETIC_TYPES_INTERNAL_H
#define _KINETIC_TYPES_INTERNAL_H

#include "kinetic_types.h"
#include "kinetic_proto.h"
#include "kinetic_countingsemaphore.h"
#include "kinetic_resourcewaiter_types.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_acl.h"
#include <netinet/in.h>
#include <ifaddrs.h>
#include <openssl/sha.h>
#include <time.h>
#include <pthread.h>

#define KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION (10)
#define KINETIC_SOCKET_DESCRIPTOR_INVALID (-1)
#define KINETIC_CONNECTION_TIMEOUT_SECS (30) /* Java simulator may take longer than 10 seconds to respond */
#define KINETIC_OPERATION_TIMEOUT_SECS (20)

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


#define NUM_ELEMENTS(ARRAY) (sizeof(ARRAY)/sizeof((ARRAY)[0]))

struct _KineticClient {
    struct bus *bus;
};

enum unpack_error {
    UNPACK_ERROR_UNDEFINED,
    UNPACK_ERROR_SUCCESS,
    UNPACK_ERROR_INVALID_HEADER,
    UNPACK_ERROR_PAYLOAD_MALLOC_FAIL,
};

// #TODO remove packed attribute and replace uses of sizeof(KineticPDUHeader)
//  with a constant
typedef struct __attribute__((__packed__)) _KineticPDUHeader {
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;

enum socket_state {
    STATE_UNINIT = 0,
    STATE_AWAITING_HEADER,
    STATE_AWAITING_BODY,
};

#define KINETIC_SEQUENCE_NOT_YET_BOUND ((int64_t)-2)

typedef struct {
    enum socket_state state;
    KineticPDUHeader header;
    enum unpack_error unpack_status;
    size_t accumulated;
    uint8_t buf[];
} socket_info;

/**
 * @brief An instance of a session with a Kinetic device.
 */
struct _KineticSession {
    KineticSessionConfig config;                        ///< session configuration which is a deep copy of client configuration supplied to KienticClient_CreateSession
    bool            connected;                          ///< state of connection
    KineticStatus   terminationStatus;                  ///< reported status upon device termination (SUCCESS if not terminated)
    int             socket;                             ///< socket file descriptor
    int64_t         connectionID;                       ///< initialized to seconds since epoch
    int64_t         sequence;                           ///< increments for each request in a session
    struct bus *    messageBus;                         ///< pointer to message bus instance
    socket_info *   si;                                 ///< pointer to socket information
    pthread_mutex_t sendMutex;                          ///< mutex for locking around seq count acquisision, PDU packing, and transfer to threadpool
    KineticResourceWaiter connectionReady;              ///< connection ready status (set to true once connectionID recieved)
    KineticCountingSemaphore * outstandingOperations;   ///< counting semaphore to only allows the configured number of outstanding operation at a given time
    uint16_t timeoutSeconds;                            ///< Default response timeout
};

// Kinetic Message HMAC
typedef struct _KineticHMAC {
    KineticProto_Command_Security_ACL_HMACAlgorithm algorithm;
    uint32_t len;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;


// Kinetic Device Message Request
typedef struct _KineticMessage {
    // Kinetic Protocol Buffer Elements

    // Base Message
    KineticProto_Message                message;
    KineticProto_Message_HMACauth       hmacAuth;
    KineticProto_Message_PINauth        pinAuth;
    uint8_t                             hmacData[KINETIC_HMAC_MAX_LEN];

    // Internal Command
    KineticProto_Command                command;
    KineticProto_Command_Header         header;
    KineticProto_Command_Body           body;
    KineticProto_Command_Status         status;
    KineticProto_Command_Security       security;
    KineticProto_Command_Security_ACL   acl;
    KineticProto_Command_KeyValue       keyValue;
    KineticProto_Command_Range          keyRange;
    KineticProto_Command_Setup          setup;
    KineticProto_Command_GetLog         getLog;
    KineticProto_Command_GetLog_Type    getLogType;
    KineticProto_Command_GetLog_Device  getLogDevice;
    KineticProto_Command_PinOperation   pinOp;
} KineticMessage;

// Kinetic PDU Header
#define PDU_HEADER_LEN              (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN           (1024 * 1024)
#define PDU_PROTO_MAX_UNPACKED_LEN  (PDU_PROTO_MAX_LEN * 2)
#define PDU_MAX_LEN                 (PDU_HEADER_LEN + \
                                    PDU_PROTO_MAX_LEN + KINETIC_OBJ_SIZE)

typedef enum {
    KINETIC_PDU_TYPE_INVALID = 0,
    KINETIC_PDU_TYPE_REQUEST,
    KINETIC_PDU_TYPE_RESPONSE,
    KINETIC_PDU_TYPE_UNSOLICITED
} KineticPDUType;


struct _KineticRequest {
    KineticMessage message;
    KineticProto_Command* command;
    bool pinAuth;
};

typedef struct _KineticResponse
{
    KineticPDUHeader header;
    KineticProto_Message* proto;
    KineticProto_Command* command;
    uint8_t value[];
} KineticResponse;

typedef struct _KineticRequest KineticRequest;
typedef struct _KineticOperation KineticOperation;

typedef KineticStatus (*KineticOperationCallback)(KineticOperation* const operation, KineticStatus const status);

// Kinetic Operation
struct _KineticOperation {
    KineticSession* session;
    KineticRequest* request;
    KineticResponse* response;
    uint16_t timeoutSeconds;
    int64_t pendingClusterVersion;
    ByteArray* pin;
    KineticEntry* entry;
    ByteBufferArray* buffers;
    KineticLogInfo** deviceInfo;
    KineticP2P_Operation* p2pOp;
    KineticOperationCallback opCallback;
    KineticCompletionClosure closure;
    ByteArray value;
};


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
struct timeval Kinetic_TimevalZero(void);
bool Kinetic_TimevalIsZero(struct timeval const tv);
struct timeval Kinetic_TimevalAdd(struct timeval const a, struct timeval const b);
int Kinetic_TimevalCmp(struct timeval const a, struct timeval const b);

KineticProto_Command_GetLog_Type KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KineticLogInfo_Type type);

KineticMessageType KineticProto_Command_MessageType_to_KineticMessageType(KineticProto_Command_MessageType type);

void KineticMessage_Init(KineticMessage* const message);
void KineticRequest_Init(KineticRequest* reqeust, KineticSession const * const session);

#endif // _KINETIC_TYPES_INTERNAL_H
