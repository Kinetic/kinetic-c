#ifndef ACL_TYPES_H
#define ACL_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "kinetic_proto.h"

typedef enum {
    HMAC_UNKNOWN = 0,
    HMAC_SHA1 = 1,
    HMAC_TYPE_COUNT,
} hmac_type_t;

struct hmac_key {
    hmac_type_t type;
    size_t length;
    uint8_t key[];
};

typedef enum {
    PERM_INVALID = -1, // place holder for backward compatibility
    PERM_READ = 0, // can read key/values
    PERM_WRITE = 1, // can write key/values
    PERM_DELETE = 2, // can delete key/values
    PERM_RANGE = 3, // can do a range
    PERM_SETUP = 4, // can set up a device
    PERM_P2POP = 5, // can do a peer to peer operation
    // [apparently 6 was deprecated]
    PERM_GETLOG = 7, // can get log
    PERM_SECURITY = 8, // can set up the security permission of the device
} permission_t;

#define ACL_MAX_PERMISSIONS 8

struct ACL {
    size_t ACL_ceil;
    size_t ACL_count;
    KineticProto_Command_Security_ACL **ACLs;
};

typedef enum {
    ACL_OK = 0,
    ACL_END_OF_STREAM = 1,
    ACL_ERROR_NULL = -1,
    ACL_ERROR_MEMORY = -2,
    ACL_ERROR_JSON_FILE = -3,
    ACL_ERROR_BAD_JSON = -4,
    ACL_ERROR_MISSING_FIELD = -5,
    ACL_ERROR_INVALID_FIELD = -6,
} acl_of_file_res;

#if 0
struct  _KineticProto_Command_Security_ACL {
    ProtobufCMessage base;
    protobuf_c_boolean has_identity;
    int64_t identity;
    protobuf_c_boolean has_key;
    ProtobufCBinaryData key;
    protobuf_c_boolean has_hmacAlgorithm;
    KineticProto_Command_Security_ACL_HMACAlgorithm hmacAlgorithm;
    size_t n_scope;
    KineticProto_Command_Security_ACL_Scope** scope;
    protobuf_c_boolean has_maxPriority;
    KineticProto_Command_Priority maxPriority;
};

struct  _KineticProto_Command_Security_ACL_Scope {
    ProtobufCMessage base;
    protobuf_c_boolean has_offset;
    int64_t offset;
    protobuf_c_boolean has_value;
    ProtobufCBinaryData value;
    size_t n_permission;
    KineticProto_Command_Security_ACL_Permission* permission;
    protobuf_c_boolean has_TlsRequired;
    protobuf_c_boolean TlsRequired;
};
#endif

#endif
