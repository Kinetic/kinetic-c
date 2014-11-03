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
#include <sys/time.h>
#include "byte_array.h"


#define KINETIC_HANDLE_INVALID  (0)
#define KINETIC_PORT            (8123)
#define KINETIC_TLS_PORT        (8443)
#define KINETIC_HMAC_SHA1_LEN   (SHA_DIGEST_LENGTH)
#define KINETIC_HMAC_MAX_LEN    (KINETIC_HMAC_SHA1_LEN)
#define KINETIC_DEFAULT_KEY_LEN (1024)
#define KINETIC_MAX_KEY_LEN     (4096)
#define KINETIC_MAX_VERSION_LEN (256)
#define KINETIC_OBJ_SIZE        (1024 * 1024)

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
typedef struct _KineticSession {
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
} KineticSession;

#define KINETIC_SESSION_INIT(_session, _host, _clusterVersion, _identity, _hmacKey) { \
    *(_session) = (KineticSession) { \
        .port = KINETIC_PORT, \
        .clusterVersion = (_clusterVersion), \
        .identity = (_identity), \
        .hmacKey = {.data = (_session)->keyData, .len = (_hmacKey).len}, \
    }; \
    strcpy((_session)->host, (_host)); \
    memcpy((_session)->hmacKey.data, (_hmacKey).data, (_hmacKey).len); \
}

// Kinetic Status Codes
typedef enum {
    KINETIC_STATUS_INVALID = -1,        // Status not available (no reponse/status available)
    KINETIC_STATUS_NOT_ATTEMPTED = 0,   // No operation has been attempted
    KINETIC_STATUS_SUCCESS = 1,         // Operation successful
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
    KINETIC_STATUS_CLUSTER_MISMATCH,    // Specified cluster version does not match device
    KINETIC_STATUS_VERSION_MISMATCH,    // The specified object version info for a PUT/GET do not match stored object
    KINETIC_STATUS_DATA_ERROR,          // Device reported data error, no space or HMAC failure
    KINETIC_STATUS_NOT_FOUND,           // The requested object does not exist
    KINETIC_STATUS_BUFFER_OVERRUN,      // One or more of byte buffers did not fit all data
    KINETIC_STATUS_MEMORY_ERROR,        // Failed allocating/deallocating memory
    KINETIC_STATUS_SOCKET_TIMEOUT,      // A timeout occurred while waiting for a socket operation
    KINETIC_STATUS_SOCKET_ERROR,        // An I/O error occurred during a socket operation
    KINETIC_STATUS_COUNT                // Number of status codes in KineticStatusDescriptor
} KineticStatus;

const char* Kinetic_GetStatusDescription(KineticStatus status);

typedef struct _KineticCompletionData {
    int64_t connectionID;
    int64_t sequence;
    struct timeval requestTime;
    KineticStatus status;
} KineticCompletionData;

typedef void (*KineticCompletionCallback)(KineticCompletionData* kinetic_data, void* client_data);

typedef struct _KineticCompletionClosure {
    KineticCompletionCallback callback;
    void* clientData;
} KineticCompletionClosure;

// KineticEntry - byte arrays need to be preallocated by the client
typedef struct _KineticEntry {
    ByteBuffer key;
    ByteBuffer value;

    // Metadata
    ByteBuffer dbVersion;
    ByteBuffer tag;
    KineticAlgorithm algorithm;

    // Operation-specific attributes (TODO: remove from struct, and specify a attributes to PUT/GET operations)
    ByteBuffer newVersion;
    bool metadataOnly;
    bool force;
    KineticSynchronization synchronization;
} KineticEntry;

// Kinetic Key Range request structure
typedef struct _KineticKeyRange {
    ByteBuffer startKey;
    ByteBuffer endKey;
    bool startKeyInclusive;
    bool endKeyInclusive;
    int32_t maxReturned;
    bool reverse;
} KineticKeyRange;

#endif // _KINETIC_TYPES_H
