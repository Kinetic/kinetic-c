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

typedef struct _KineticHMAC
{
    KineticProto_Security_ACL_HMACAlgorithm algorithm;
    unsigned int valueLength;
    uint8_t value[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;

typedef struct _KineticConnection
{
    bool    connected;
    bool    blocking;
    int     port;
    int     socketDescriptor;
    char    host[HOST_NAME_MAX];
} KineticConnection;

#define KINETIC_CONNECTION_INIT(connection) { \
    memset((connection), 0, sizeof(KineticConnection)); \
    (connection)->blocking = true; \
    (connection)->socketDescriptor = -1; \
}

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

#define KINETIC_MESSAGE_INIT(msg) { \
    KineticProto_init(&(msg)->proto); \
    KineticProto_command_init(&(msg)->command); \
    KineticProto_header_init(&(msg)->header); \
    KineticProto_status_init(&(msg)->status); \
    KineticProto_body_init(&(msg)->body); \
    KineticProto_key_value_init(&(msg)->keyValue); \
    memset((msg)->hmacData, 0, SHA_DIGEST_LENGTH); \
    (msg)->proto.hmac.data = (msg)->hmacData; \
    (msg)->proto.hmac.len = KINETIC_HMAC_MAX_LEN; \
    (msg)->proto.has_hmac = true; /* Enable HMAC to allow length calculation prior to population */ \
    (msg)->command.header = &(msg)->header; \
    (msg)->proto.command = &(msg)->command; \
}


typedef struct _KineticExchange
{
    // Defaults to false, since clusterVersion is optional
    // Set to true to enable clusterVersion for the exchange
    bool has_clusterVersion;

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
    bool has_key;
    size_t keyLength;
    char key[KINETIC_MAX_KEY_LEN+1];

    // Required field
    // A unique number for this connection between the source and target.
    // On the first request to the drive, this should be the time of day in
    // seconds since 1970. The drive can change this number and the client must
    // continue to use the new number and the number must remain constant
    // during the session
    int64_t connectionID;

    // Required field
    // A monotonically increasing number for each request in a TCP connection.
    int64_t sequence;

    // Associated Kinetic connection
    KineticConnection* connection;
} KineticExchange;

#define KINETIC_EXCHANGE_INIT(exchg, id, k, klen, con) { \
    memset((exchg), 0, sizeof(KineticExchange)); \
    (exchg)->identity = id; \
    if (k != NULL && klen > 0) \
    { \
        memcpy((exchg)->key, k, klen); \
        (exchg)->key[klen] = '\0'; \
        (exchg)->keyLength = klen; \
        (exchg)->has_key = true; \
    } \
    (exchg)->connection = con; \
    (exchg)->sequence = -1; \
}




#define PDU_HEADER_LEN      (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN   (1024 * 1024)
#define PDU_VALUE_MAX_LEN   (1024 * 1024)
#define PDU_MAX_LEN         (PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)

typedef struct __attribute__ ((__packed__)) _KineticPDUHeader
{
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;

typedef struct _KineticPDU
{
    // Binary PDU header (binary packed in NBO)
    KineticPDUHeader header;
    uint8_t rawHeader[sizeof(KineticPDUHeader)];

    // Message associated with this PDU instance
    KineticMessage* message;
    KineticProto* proto;
    uint32_t protobufLength; // Embedded in header in NBO byte order (this is for reference)
    uint8_t protobufScratch[1024*1024];

    // Value data associated with PDU (if any)
    uint8_t* value;
    uint32_t valueLength; // Embedded in header in NBO byte order (this is for reference)

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticExchange* exchange;
} KineticPDU;

typedef struct _KineticOperation
{
    KineticExchange* exchange;
    KineticPDU* request;
    KineticPDU* response;
} KineticOperation;

#define KINETIC_OPERATION_INIT(op, xchng, req, resp) { \
    (op)->exchange = (xchng); \
    (op)->request = (req); \
    (op)->response = (resp); \
}

#endif // _KINETIC_TYPES_H
