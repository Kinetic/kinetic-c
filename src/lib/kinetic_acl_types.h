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
    size_t ACL_ceil;
    size_t ACL_count;
    Com__Seagate__Kinetic__Proto__Command__Security__ACL **ACLs;
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
