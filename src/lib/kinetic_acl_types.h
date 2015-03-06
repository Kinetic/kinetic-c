#ifndef KINETIC_ACL_TYPES_H
#define KINETIC_ACL_TYPES_H

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

struct ACL {
    size_t ACL_ceil;
    size_t ACL_count;
    KineticProto_Command_Security_ACL **ACLs;
};

#define ACL_MAX_PERMISSIONS 8

typedef enum {
    ACL_OK = 0,
    ACL_END_OF_STREAM = 1,
    ACL_ERROR_NULL = -1,
    ACL_ERROR_MEMORY = -2,
    ACL_ERROR_JSON_FILE = -3,
    ACL_ERROR_BAD_JSON = -4,
    ACL_ERROR_MISSING_FIELD = -5,
    ACL_ERROR_INVALID_FIELD = -6,
} KineticACLLoadResult;

#endif // KINETIC_ACL_TYPES_H
