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

#ifndef KINETIC_ACL_TYPES_H
#define KINETIC_ACL_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "kinetic.pb-c.h"

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
    size_t ACL_ceil;     ///< Ceiling of ACLs array: resize if count == ceil.
    size_t ACL_count;    ///< How many ACL * structs are in ACLs[].
    Com__Seagate__Kinetic__Proto__Command__Security__ACL **ACLs;  ///< ACL struct array.
};

#define ACL_MAX_PERMISSIONS 8

typedef enum {
    ACL_OK = 0,                      ///< Okay
    ACL_END_OF_STREAM = 1,           ///< End of stream
    ACL_ERROR_NULL = -1,             ///< NULL pointer error
    ACL_ERROR_MEMORY = -2,           ///< Memory allocation failure
    ACL_ERROR_JSON_FILE = -3,        ///< Unable to open JSON file
    ACL_ERROR_BAD_JSON = -4,         ///< Invalid JSON in file
    ACL_ERROR_MISSING_FIELD = -5,    ///< Missing required field
    ACL_ERROR_INVALID_FIELD = -6,    ///< Invalid field
} KineticACLLoadResult;

#endif // KINETIC_ACL_TYPES_H
