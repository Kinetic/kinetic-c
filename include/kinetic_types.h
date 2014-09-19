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

// #include <netinet/in.h>
// #include <ifaddrs.h>


#define KINETIC_HANDLE_INVALID  (0)
#define KINETIC_PORT            (8123)
#define KINETIC_TLS_PORT        (8443)
#define KINETIC_HMAC_SHA1_LEN   (SHA_DIGEST_LENGTH)
#define KINETIC_HMAC_MAX_LEN    (KINETIC_HMAC_SHA1_LEN)
#define KINETIC_MAX_KEY_LEN     (128)
#define PDU_VALUE_MAX_LEN       (1024 * 1024)

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

#ifndef LOG_FILE_NAME_MAX
#define LOG_FILE_NAME_MAX (HOST_NAME_MAX)
#endif

/**
 * @brief Structure for handling generic arrays of bytes
 *
 * The data contained in a `ByteArray` is an arbitrary sequence of
 * bytes. It may contain embedded `NULL` characters and is not required to be
 * `NULL`-terminated.
 */
typedef struct _ByteArray {
    size_t  len;    /**< Number of bytes in the `data` field. */
    uint8_t *data;  /**< Pointer to an allocated array of data bytes. */
} ByteArray;

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


/**
 * @brief Structure for an embedded ByteArray as a buffer
 *
 * The `bytesUsed` field is initialized to zero, and is to incremented as each
 * byte is consumed, but shall not exceed the `array` length
 */
typedef struct
{
    ByteArray   array;
    size_t      bytesUsed;
} ByteBuffer;
#define BYTE_BUFFER_INIT(_array) (ByteBuffer) { \
    .array = (ByteArray) { \
        .data = (_array).data, \
        .len = (_array).len }, \
    .bytesUsed = 0, \
}

/**
 * @brief Enumeration of encryption/checksum key algorithms
 */
typedef enum _KineticAlgorithm {
    KINETIC_ALGORITHM_INVALID = -1,
    KINETIC_ALGORITHM_SHA1 = 2,
    KINETIC_ALGORITHM_SHA2,
    KINETIC_ALGORITHM_SHA3,
    KINETIC_ALGORITHM_CRC32,
    KINETIC_ALGORITHM_CRC64
} KineticAlgorithm;


/**
 * @brief Enumeration of synchronization types for an operation.
 */
typedef enum _KineticSynchronization {
  KINETIC_SYNCHRONIZATION_INVALID = -1,
  KINETIC_SYNCHRONIZATION_WRITETHROUGH = 1,
  KINETIC_SYNCHRONIZATION_WRITEBACK = 2,
  KINETIC_SYNCHRONIZATION_FLUSH = 3
} KineticSynchronization;


/**
 * @brief Handle for a session instance
 */
typedef int KineticSessionHandle;


/**
 * @brief Structure used to specify the configuration of a session.
 */
typedef struct _KineticSession
{
    // Host name/IP address of Kinetic Device
    char    host[HOST_NAME_MAX];

    // Port for Kinetic Device session
    int     port;

    // Set to true to enable non-blocking/asynchronous I/O
    bool    nonBlocking;

    // The version number of this cluster definition. If this is not equal to
    // the value on the Kinetic Device, the request is rejected and will return
    // `KINETIC_STATUS_VERSION_FAILURE`
    int64_t clusterVersion;

    // The identity associated with this request. See the ACL discussion above.
    // The Kinetic Device will use this identity value to lookup the
    // HMAC key (shared secret) to verify the HMAC.
    int64_t identity;

    // This is the identity's HMAC Key. This is a shared secret between the
    // client and the device, used to sign requests.
    uint8_t keyData[KINETIC_MAX_KEY_LEN];
    ByteArray hmacKey;

    // Log file name (uses stdout if empty)
    char    logFile[LOG_FILE_NAME_MAX];
} KineticSession;

#define KINETIC_SESSION_INIT(_session, \
    _host, _clusterVersion, _identity, _hmacKey) { \
    *(_session) = (KineticSession) { \
        .logFile = "", \
        .port = KINETIC_PORT, \
        .clusterVersion = (_clusterVersion), \
        .identity = (_identity), \
        .hmacKey = {.data = (_session)->keyData, .len = (_hmacKey).len}, \
    }; \
    strcpy((_session)->host, (_host)); \
    memcpy((_session)->hmacKey.data, (_hmacKey).data, (_hmacKey).len); \
}

// Operation handle
typedef int KineticOperationHandle;

// Kinetic Status Codes
typedef enum
{
    KINETIC_STATUS_INVALID = -1,        // Status not available (no reponse/status available)
    KINETIC_STATUS_SUCCESS = 0,         // Operation successful
    KINETIC_STATUS_SESSION_EMPTY,       // Session was NULL in request
    KINETIC_STATUS_SESSION_INVALID,     // Session configuration was invalid or NULL
    KINETIC_STATUS_HOST_EMPTY,          // Host was empty in request
    KINETIC_STATUS_HMAC_EMPTY,          // HMAC key is empty or NULL
    KINETIC_STATUS_NO_PDUS_AVAVILABLE,  // All PDUs for the session have been allocated
    KINETIC_STATUS_DEVICE_BUSY,         // Device busy (retry later)
    KINETIC_STATUS_CONNECTION_ERROR,    // No connection/disconnected
    KINETIC_STATUS_INVALID_REQUEST,     // Something about the request is invalid
    KINETIC_STATUS_OPERATION_INVALID,   // Operation was invalid
    KINETIC_STATUS_OPERATION_FAILED,    // Device reported an operation error
    KINETIC_STATUS_VERSION_FAILURE,     // Basically a VERSION_MISMATCH error for a PUT
    KINETIC_STATUS_DATA_ERROR,          // Device reported data error, no space or HMAC failure
    KINETIC_STATUS_COUNT                // Number of status codes in KineticStatusDescriptor
} KineticStatus;
extern const char* KineticStatusDescriptor[];


// KeyValue data
typedef struct _KineticKeyValue
{
    ByteArray key;
    ByteArray newVersion;
    ByteArray dbVersion;
    ByteArray tag;
    bool force;
    KineticAlgorithm algorithm;
    bool metadataOnly;
    KineticSynchronization synchronization;
    ByteArray value;
} KineticKeyValue;

// Expose normally private data for test builds to allow inspection
#ifdef TEST
#define STATIC
#else
#define STATIC static
#endif


#endif // _KINETIC_TYPES_H
