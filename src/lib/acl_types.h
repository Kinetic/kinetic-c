#ifndef ACL_TYPES_H
#define ACL_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

struct acl_scope {
    int64_t offset;
    size_t valueSize;
    uint8_t *value;
    uint8_t permission_count;
    permission_t permissions[ACL_MAX_PERMISSIONS];
    bool tlsRequired;
};

#define ACL_NO_IDENTITY (-1)
#define ACL_NO_OFFSET (-1)

struct ACL {
    int64_t identity;
    struct hmac_key *hmacKey;
    size_t scopeCount;
    struct acl_scope scopes[];
};

typedef enum {
    ACL_OK = 0,
    ACL_ERROR_NULL = -1,
    ACL_ERROR_MEMORY = -2,
    ACL_ERROR_BAD_JSON = -3,
    ACL_ERROR_MISSING_FIELD = -4,
    ACL_ERROR_INVALID_FIELD = -5,
} acl_of_file_res;

#endif
