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


#define KINETIC_SOCKET_INVALID  (-1)                    ///< Invalid socket file descriptor value
#define KINETIC_PORT            (8123)                  ///< Default kinetic port
#define KINETIC_TLS_PORT        (8443)                  ///< Default kinetic TLS port
#define KINETIC_HMAC_SHA1_LEN   (SHA_DIGEST_LENGTH)     ///< HMAC secure hash length
#define KINETIC_HMAC_MAX_LEN    (KINETIC_HMAC_SHA1_LEN) ///< HMAC max length
#define KINETIC_PIN_MAX_LEN     (1024)                  ///< Max PIN length
#define KINETIC_DEFAULT_KEY_LEN (1024)                  ///< Default key length
#define KINETIC_MAX_KEY_LEN     (4096)                  ///< Max key length
#define KINETIC_OBJ_SIZE        (1024 * 1024)           ///< Max object/value size

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


/**
 * @brief kinetic-c library version info (returned from KineticClient_Version())
 */
 typedef struct {
    const char * version;           ///< kinetic-c library version
    const char * protocolVersion;   ///< kinetic-protocol version library is built with
    const char * repoCommitHash;    ///< repository commit hash which library is built from
 } KineticVersionInfo;

/**
 * @brief Enumeration of encryption/checksum key algorithms
 */
typedef enum _KineticAlgorithm {
    KINETIC_ALGORITHM_INVALID = -1, ///< Invalid algorithm value
    KINETIC_ALGORITHM_SHA1 = 2,     ///< SHA1
    KINETIC_ALGORITHM_SHA2 = 3,     ///< SHA2
    KINETIC_ALGORITHM_SHA3 = 4,     ///< SHA3
    KINETIC_ALGORITHM_CRC32 = 5,    ///< CRC32
    KINETIC_ALGORITHM_CRC64 = 6     ///< CRC64
} KineticAlgorithm;


/**
 * @brief Enumeration of synchronization types for an operation on a `KineticEntry`.
 */
typedef enum _KineticSynchronization {

    /// Invalid synchronization value
    KINETIC_SYNCHRONIZATION_INVALID = -1,

    /// This request is made persistent before returning.
    /// This does not effect any other pending operations.
    KINETIC_SYNCHRONIZATION_WRITETHROUGH = 1,

    /// They can be made persistent when the drive chooses,
    /// or when a subsequent FLUSH is sent to the drive.
    KINETIC_SYNCHRONIZATION_WRITEBACK = 2,

    /// All pending information that has not been written is
    /// pushed to the disk and the command that specifies
    /// FLUSH is written last and then returned. All WRITEBACK writes
    /// that have received ending status will be guaranteed to be
    /// written before the FLUSH operation is returned completed.
    KINETIC_SYNCHRONIZATION_FLUSH = 3
} KineticSynchronization;


struct _KineticClient;
/**
 * @brief Handle to the kinetic client, which is shared by all connections
 */
typedef struct _KineticClient KineticClient;


/**
 * @brief Structure used to specify the configuration for a session.
 */
typedef struct _KineticSessionConfig {
    /// Host name/IP address of Kinetic Device
    char    host[HOST_NAME_MAX];

    /// Port for Kinetic Device session
    int     port;

    /// The version number of this cluster definition. If this is not equal to
    /// the value on the Kinetic Device, the request is rejected and will return
    /// `KINETIC_STATUS_VERSION_FAILURE`
    int64_t clusterVersion;

    /// The identity associated with this request. See the ACL discussion above.
    /// The Kinetic Device will use this identity value to lookup the
    /// HMAC key (shared secret) to verify the HMAC.
    int64_t identity;

    /// This is the identity's HMAC Key. This is a shared secret between the
    /// client and the device, used to sign requests.
    uint8_t keyData[KINETIC_MAX_KEY_LEN];
    ByteArray hmacKey;

    /// Set to `true' to enable SSL for for this session
    bool useSsl;

    /// Operation timeout. If 0, use the default (10 seconds).
    uint16_t timeoutSeconds;
} KineticSessionConfig;

/**
 * @brief An instance of a session with a Kinetic device.
 */
typedef struct _KineticSession KineticSession;

/**
* @brief Kinetic status codes.
*/
typedef enum {
    KINETIC_STATUS_INVALID = -1,            ///< Status not available (no reponse/status available)
    KINETIC_STATUS_NOT_ATTEMPTED = 0,       ///< No operation has been attempted
    KINETIC_STATUS_SUCCESS = 1,             ///< Operation successful
    KINETIC_STATUS_SESSION_EMPTY,           ///< Session was NULL in request
    KINETIC_STATUS_SESSION_INVALID,         ///< Session configuration was invalid or NULL
    KINETIC_STATUS_HOST_EMPTY,              ///< Host was empty in request
    KINETIC_STATUS_HMAC_REQUIRED,           ///< HMAC key is empty or NULL
    KINETIC_STATUS_NO_PDUS_AVAILABLE,      ///< All PDUs for the session have been allocated
    KINETIC_STATUS_DEVICE_BUSY,             ///< Device busy (retry later)
    KINETIC_STATUS_CONNECTION_ERROR,        ///< No connection/disconnected
    KINETIC_STATUS_INVALID_REQUEST,         ///< Something about the request is invalid
    KINETIC_STATUS_OPERATION_INVALID,       ///< Operation was invalid
    KINETIC_STATUS_OPERATION_FAILED,        ///< Device reported an operation error
    KINETIC_STATUS_OPERATION_TIMEDOUT,      ///< Device did not respond to the operation in time
    KINETIC_STATUS_CLUSTER_MISMATCH,        ///< Specified cluster version does not match device
    KINETIC_STATUS_VERSION_MISMATCH,        ///< The specified object version info for a PUT/GET do not match stored object
    KINETIC_STATUS_DATA_ERROR,              ///< Device reported data error, no space or HMAC failure
    KINETIC_STATUS_NOT_FOUND,               ///< The requested object does not exist
    KINETIC_STATUS_BUFFER_OVERRUN,          ///< One or more of byte buffers did not fit all data
    KINETIC_STATUS_MEMORY_ERROR,            ///< Failed allocating/deallocating memory
    KINETIC_STATUS_SOCKET_TIMEOUT,          ///< A timeout occurred while waiting for a socket operation
    KINETIC_STATUS_SOCKET_ERROR,            ///< An I/O error occurred during a socket operation
    KINETIC_STATUS_MISSING_KEY,             ///< An operation is missing a required key
    KINETIC_STATUS_MISSING_VALUE_BUFFER,    ///< An operation is missing a required value buffer
    KINETIC_STATUS_MISSING_PIN,             ///< An operation is missing a PIN
    KINETIC_STATUS_SSL_REQUIRED,            ///< The operation requires an SSL connection and the specified connection is non-SSL
    KINETIC_STATUS_DEVICE_LOCKED,           ///< The operation failed because the device is securely locked. An UNLOCK must be issued to unlock for use.
    KINETIC_STATUS_ACL_ERROR,               ///< A security operation failed due to bad ACL(s)
    KINETIC_STATUS_NOT_AUTHORIZED,          ///< Authorization failure
    KINETIC_STATUS_INVALID_FILE,            ///< Specified file does not exist or could not be read/writtten
    KINETIC_STATUS_REQUEST_REJECTED,        ///< No request was attempted.
    KINETIC_STATUS_DEVICE_NAME_REQUIRED,    ///< A device name is required, but was empty
    KINETIC_STATUS_INVALID_LOG_TYPE,        ///< The device log type specified was invalid
    KINETIC_STATUS_HMAC_FAILURE,            ///< An HMAC validation error was detected
    KINETIC_STATUS_SESSION_TERMINATED,      ///< The session has been terminated by the Kinetic device
    KINETIC_STATUS_COUNT                    ///< Number of status codes in KineticStatusDescriptor
} KineticStatus;

/**
 * @brief Provides a string representation for a KineticStatus code.
 *
 * @param status    The status enumeration value.
 *
 * @return          Pointer to the appropriate string representation for the specified status.
 */
const char* Kinetic_GetStatusDescription(KineticStatus status);

/**
 * @brief Completion data which will be provided to KineticCompletionClosure for asynchronous operations.
 */
typedef struct _KineticCompletionData {
    int64_t connectionID;       ///< Connection ID for the session
    int64_t sequence;           ///< Sequence count for the operation
    struct timeval requestTime; ///< Time at which the operation request was queued up
    KineticStatus status;       ///< Resultant status of the operation
} KineticCompletionData;

/**
 * @brief Operation completion callback function prototype.
 *
 * @param kinetic_data  KineticCompletionData provided by kinetic-c.
 * @param client_data   Optional pointer to arbitrary client-supplied data.
 */
typedef void (*KineticCompletionCallback)(KineticCompletionData* kinetic_data, void* client_data);


/**
 * @brief Closure which can be specified for operations which support asynchronous mode
 */
typedef struct _KineticCompletionClosure {
    KineticCompletionCallback callback; ///< Function to be called upon completion
    void* clientData;                   ///< Optional client-supplied data which will be supplied to callback
} KineticCompletionClosure;

/**
 * @brief Kinetic object instance
 *
 * The ByteBuffer attributes must be allocated and freed by the client, if used.
 */
typedef struct _KineticEntry {
    ByteBuffer key;             ///< Key associated with the object stored on disk
    ByteBuffer value;           ///< Value data associated with the key

    // Metadata
    ByteBuffer dbVersion;       ///< Current version of the entry (optional)
    ByteBuffer tag;             ///< Generated authentication hash per the specified `algorithm`
    KineticAlgorithm algorithm; ///< Algorithm used to generate the specified `tag`

    // Operation-specific attributes (TODO: remove from struct, and specify a attributes to PUT/GET operations)
    ByteBuffer newVersion;      ///< New version for the object to assume once written to disk (optional)
    bool metadataOnly;          ///< If set for a GET request, will return only the metadata for the specified object (`value` will not be retrieved)
    bool force;                 ///< If set for a GET/DELETE request, will override `version` checking
    bool computeTag;            ///< If set and an algorithm is specified, the tag will be populated with the calculated hash for integrity checking
    KineticSynchronization synchronization; ///< Synchronization method to use for PUT/DELETE requests.
} KineticEntry;

/**
 * @brief Kinetic Key Range request structure
 */
typedef struct _KineticKeyRange {

    /// Required bytes, the beginning of the requested range
    ByteBuffer startKey;

    /// Required bytes, the end of the requested range
    ByteBuffer endKey;

    /// Optional bool, defaults to false
    /// If set, indicates that the start key should be included in the returned range
    bool startKeyInclusive;

    /// Optional bool, defaults to false
    /// If set, indicates that the end key should be included in the returned range
    bool endKeyInclusive;

    /// Required int32, must be greater than 0
    /// The maximum number of keys returned, in sorted order
    int32_t maxReturned;

    /// Optional bool, defaults to false
    /// If true, the key range will be returned in reverse order, starting at
    /// endKey and moving back to startKey.  For instance
    /// if the search is startKey="j", endKey="k", maxReturned=2,
    /// reverse=true and the keys "k0", "k1", "k2" exist
    /// the system will return "k2" and "k1" in that order.
    bool reverse;
} KineticKeyRange;

// Kinetic GetLog data types

/**
 * @brief Log info type
 */
typedef enum {
    KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS = 0,
    KINETIC_DEVICE_INFO_TYPE_TEMPERATURES,
    KINETIC_DEVICE_INFO_TYPE_CAPACITIES,
    KINETIC_DEVICE_INFO_TYPE_CONFIGURATION,
    KINETIC_DEVICE_INFO_TYPE_STATISTICS,
    KINETIC_DEVICE_INFO_TYPE_MESSAGES,
    KINETIC_DEVICE_INFO_TYPE_LIMITS
} KineticLogInfo_Type;

/**
 * @brief Log info untilization entry
 */
typedef struct {
    char* name;
    float value;
} KineticLogInfo_Utilization;

/**
 * Log info temperature entry
 */
typedef struct {
    char* name;
    float current;
    float minimum;
    float maximum;
    float target;
} KineticLogInfo_Temperature;

/**
 * Log info capacity entry
 */
typedef struct {
    uint64_t nominalCapacityInBytes;
    float portionFull;
} KineticLogInfo_Capacity;

/**
 * Log info network interface entry
 */
typedef struct {
    char* name;
    ByteArray MAC;
    ByteArray ipv4Address;
    ByteArray ipv6Address;
} KineticLogInfo_Interface;

/**
 * Log info device configuration
 */
typedef struct {
    char* vendor;
    char* model;
    ByteArray serialNumber;
    ByteArray worldWideName;
    char* version;
    char* compilationDate;
    char* sourceHash;
    char* protocolVersion;
    char* protocolCompilationDate;
    char* protocolSourceHash;
    KineticLogInfo_Interface* interfaces;
    size_t numInterfaces;
    int32_t port;
    int32_t tlsPort;
} KineticLogInfo_Configuration;

/**
 * Log info message types
 */
typedef enum {
    KINETIC_MESSAGE_TYPE_INVALID = -1,
    KINETIC_MESSAGE_TYPE_GET_RESPONSE = 1,              ///< GET_RESPONSE
    KINETIC_MESSAGE_TYPE_GET = 2 ,                      ///< GET
    KINETIC_MESSAGE_TYPE_PUT_RESPONSE = 3,              ///< PUT_RESPONSE
    KINETIC_MESSAGE_TYPE_PUT = 4,                       ///< PUT
    KINETIC_MESSAGE_TYPE_DELETE_RESPONSE = 5,           ///< DELETE_RESPONSE
    KINETIC_MESSAGE_TYPE_DELETE = 6,                    ///< DELETE
    KINETIC_MESSAGE_TYPE_GETNEXT_RESPONSE = 7,          ///< GETNEXT_RESPONSE
    KINETIC_MESSAGE_TYPE_GETNEXT = 8,                   ///< GETNEXT
    KINETIC_MESSAGE_TYPE_GETPREVIOUS_RESPONSE = 9,      ///< GETPREVIOUS_RESPONSE
    KINETIC_MESSAGE_TYPE_GETPREVIOUS = 10,               ///< GETPREVIOUS
    KINETIC_MESSAGE_TYPE_GETKEYRANGE_RESPONSE = 11,      ///< GETKEYRANGE_RESPONSE
    KINETIC_MESSAGE_TYPE_GETKEYRANGE = 12,               ///< GETKEYRANGE
    KINETIC_MESSAGE_TYPE_GETVERSION_RESPONSE = 15,       ///< GETVERSION_RESPONSE
    KINETIC_MESSAGE_TYPE_GETVERSION = 16,                ///< GETVERSION
    KINETIC_MESSAGE_TYPE_SETUP_RESPONSE = 21,            ///< SETUP_RESPONSE
    KINETIC_MESSAGE_TYPE_SETUP = 22,                     ///< SETUP
    KINETIC_MESSAGE_TYPE_GETLOG_RESPONSE = 23,           ///< GETLOG_RESPONSE
    KINETIC_MESSAGE_TYPE_GETLOG = 24,                    ///< GETLOG
    KINETIC_MESSAGE_TYPE_SECURITY_RESPONSE = 25,         ///< SECURITY_RESPONSE
    KINETIC_MESSAGE_TYPE_SECURITY = 26,                  ///< SECURITY
    KINETIC_MESSAGE_TYPE_PEER2PEERPUSH_RESPONSE = 27,    ///< PEER2PEERPUSH_RESPONSE
    KINETIC_MESSAGE_TYPE_PEER2PEERPUSH = 28,             ///< PEER2PEERPUSH
    KINETIC_MESSAGE_TYPE_NOOP_RESPONSE = 29,             ///< NOOP_RESPONSE
    KINETIC_MESSAGE_TYPE_NOOP = 30,                      ///< NOOP
    KINETIC_MESSAGE_TYPE_FLUSHALLDATA_RESPONSE = 31,     ///< FLUSHALLDATA_RESPONSE
    KINETIC_MESSAGE_TYPE_FLUSHALLDATA = 32,              ///< FLUSHALLDATA
    KINETIC_MESSAGE_TYPE_PINOP_RESPONSE = 35,            ///< PINOP_RESPONSE
    KINETIC_MESSAGE_TYPE_PINOP = 36,                     ///< PINOP
    KINETIC_MESSAGE_TYPE_MEDIASCAN_RESPONSE = 37,        ///< MEDIASCAN_RESPONSE
    KINETIC_MESSAGE_TYPE_MEDIASCAN = 38,                 ///< MEDIASCAN
    KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE_RESPONSE = 39,    ///< MEDIAOPTIMIZE_RESPONSE
    KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE = 40,             ///< MEDIAOPTIMIZE
} KineticMessageType;

/**
 * @brief Log info statistics entry
 */
typedef struct {
    KineticMessageType messageType;
    uint64_t count;
    uint64_t bytes;
} KineticLogInfo_Statistics;

/**
 * @brief Log info device limits
 */
typedef struct {
    uint32_t maxKeySize;
    uint32_t maxValueSize;
    uint32_t maxVersionSize;
    uint32_t maxTagSize;
    uint32_t maxConnections;
    uint32_t maxOutstandingReadRequests;
    uint32_t maxOutstandingWriteRequests;
    uint32_t maxMessageSize;
    uint32_t maxKeyRangeCount;
    uint32_t maxIdentityCount;
    uint32_t maxPinSize;
} KineticLogInfo_Limits;

/**
 * Log info device name (used as a key for device-specific log data)
 */
typedef struct {
    ByteArray name;
} KineticLogInfo_Device;

/**
 * Base log info structure which is allocated by client and passed to KineticAdminClient_GetLog
 */
typedef struct {
    KineticLogInfo_Utilization* utilizations;
    size_t numUtilizations;
    KineticLogInfo_Temperature* temperatures;
    size_t numTemperatures;
    KineticLogInfo_Capacity* capacity;
    KineticLogInfo_Configuration* configuration;
    KineticLogInfo_Statistics* statistics;
    size_t numStatistics;
    ByteArray messages;
    KineticLogInfo_Limits* limits;
    KineticLogInfo_Device* device;
} KineticLogInfo;

/**
 * Configuration of remote peer for a PEER2PEERPUSH operation
 */
typedef struct {
    char*   hostname;   ///< Host name of peer/destination. Pointer must remain valid until operation completes
    int32_t port;       ///< Port to etablish peer connection to destination host.
    bool    tls;        ///< If set, will use TLS for peer connection. Optional, defaults to false
} KineticP2P_Peer;

/**
 * @brief Peer-to-peer opearation instance
 */
typedef struct _KineticP2P_Operation KineticP2P_Operation;

/**
 * @brief Peer-to-peer operation data structure
 */
typedef struct {
    ByteBuffer    key;
    ByteBuffer    version; // optional (defaults to force if not specified)
    ByteBuffer    newKey;
    KineticP2P_Operation* chainedOperation;
    KineticStatus resultStatus; // populated with the result of the operation
} KineticP2P_OperationData;

struct _KineticP2P_Operation {
    KineticP2P_Peer peer; ///> Peer to perform operations with
    size_t numOperations; ///> Number of operations in operations array
    KineticP2P_OperationData* operations; ///> Pointer to operations array which must remain valid until operations complete
};

typedef struct KineticMedia_Operation{
	char* start_key;
	char* end_key;
	bool start_key_inclusive;
	bool end_key_inclusive;
}KineticMediaScan_Operation, KineticMediaOptimize_Operation;

typedef enum _KineticCommand_Priority{
    PRIORITY_NORMAL,
    PRIORITY_LOWEST,
    PRIORITY_LOWER,
    PRIORITY_HIGHER,
    PRIORITY_HIGHEST
} KineticCommand_Priority;

/**
 * @brief Limit for P2P operations.
 */
#define KINETIC_P2P_OPERATION_LIMIT 100000

/**
 * @brief Limit for P2P operation nesting.
 */
#define KINETIC_P2P_MAX_NESTING 1000

/**
 * @brief Default values for the KineticClientConfig struct, which will be used
 * if the corresponding field in the struct is 0.
 */
#define KINETIC_CLIENT_DEFAULT_LOG_LEVEL 0
#define KINETIC_CLIENT_DEFAULT_READER_THREADS 4
#define KINETIC_CLIENT_DEFAULT_MAX_THREADPOOL_THREADS 8

/**
 * @brief Configuration values for the KineticClient connection.
 *
 * Configuration for the KineticClient connection. If fields are zeroed out, default
 * values will be used.
 */
typedef struct {
    const char *logFile;            ///< Path to log file. Specify 'stdout' to log to STDOUT or NULL to disable logging.
    int logLevel;                   ///< Logging level (-1:none, 0:error, 1:info, 2:verbose, 3:full)
    uint8_t readerThreads;          ///< Number of threads used for handling incoming responses and status messages
    uint8_t maxThreadpoolThreads;   ///< Max number of threads to use for the threadpool that handles response callbacks.
} KineticClientConfig;

/**
 * @brief Provides a string representation for a Kinetic message type.
 *
 * @param type  The message type value.
 *
 * @return      Pointer to the appropriate string representation for the specified type.
 */
const char* KineticMessageType_GetName(KineticMessageType type);

#endif // _KINETIC_TYPES_H
