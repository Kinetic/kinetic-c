/*
* kinetic-c-client
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

#include "kinetic_types_internal.h"
#include "kinetic_types.h"

STATIC const char* KineticStatusInvalid = "INVALID_STATUS_CODE";
STATIC const char* KineticStatusDescriptor[] = {
    "NOT_ATTEMPTED",
    "SUCCESS",
    "SESSION_EMPTY",
    "SESSION_INVALID",
    "HOST_EMPTY",
    "HMAC_REQUIRED",
    "NO_PDUS_AVAVILABLE",
    "DEVICE_BUSY",
    "CONNECTION_ERROR",
    "INVALID_REQUEST",
    "OPERATION_INVALID",
    "OPERATION_FAILED",
    "OPERATION_TIMEDOUT",
    "CLUSTER_MISMATCH",
    "VERSION_MISMATCH",
    "DATA_ERROR",
    "NOT_FOUND",
    "BUFFER_OVERRUN",
    "MEMORY_ERROR",
    "SOCKET_TIMEOUT",
    "SOCKET_ERROR",
    "MISSING_KEY",
    "MISSING_VALUE_BUFFER",
    "PIN_REQUIRED",
    "SSL_REQUIRED",
};

#ifdef TEST
STATIC const int KineticStatusDescriptorCount =
    sizeof(KineticStatusDescriptor) / sizeof(char*);
#endif

const char* Kinetic_GetStatusDescription(KineticStatus status)
{
    if ((int)status < 0 || status >= (int)KINETIC_STATUS_COUNT) {
        return KineticStatusInvalid;
    }
    return KineticStatusDescriptor[(int)status];
}

const char* KineticMessageTypeNames[33] =
{
    "<UNKNOWN>"
    "GET_RESPONSE",
    "GET",
    "PUT_RESPONSE",
    "PUT",
    "DELETE_RESPONSE",
    "DELETE",
    "GETNEXT_RESPONSE",
    "GETNEXT",
    "GETPREVIOUS_RESPONSE",
    "GETPREVIOUS",
    "GETKEYRANGE_RESPONSE",
    "GETKEYRANGE",
    "GETVERSION_RESPONSE",
    "GETVERSION",
    "SETUP_RESPONSE",
    "SETUP",
    "GETLOG_RESPONSE",
    "GETLOG",
    "SECURITY_RESPONSE",
    "SECURITY",
    "PEER2PEERPUSH_RESPONSE",
    "PEER2PEERPUSH",
    "NOOP_RESPONSE",
    "NOOP",
    "FLUSHALLDATA_RESPONSE",
    "FLUSHALLDATA",
    "PINOP_RESPONSE",
    "PINOP",
    "MEDIASCAN_RESPONSE",
    "MEDIASCAN",
    "MEDIAOPTIMIZE_RESPONSE",
    "MEDIAOPTIMIZE"
};

const char* KineticMessageType_GetName(KineticMessageType type)
{
    switch(type) {
    case KINETIC_MESSAGE_TYPE_GET_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GET:
    case KINETIC_MESSAGE_TYPE_PUT_RESPONSE:
    case KINETIC_MESSAGE_TYPE_PUT:
    case KINETIC_MESSAGE_TYPE_DELETE_RESPONSE:
    case KINETIC_MESSAGE_TYPE_DELETE:
    case KINETIC_MESSAGE_TYPE_GETNEXT_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GETNEXT:
    case KINETIC_MESSAGE_TYPE_GETPREVIOUS_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GETPREVIOUS:
    case KINETIC_MESSAGE_TYPE_GETKEYRANGE_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GETKEYRANGE:
    case KINETIC_MESSAGE_TYPE_GETVERSION_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GETVERSION:
    case KINETIC_MESSAGE_TYPE_SETUP_RESPONSE:
    case KINETIC_MESSAGE_TYPE_SETUP:
    case KINETIC_MESSAGE_TYPE_GETLOG_RESPONSE:
    case KINETIC_MESSAGE_TYPE_GETLOG:
    case KINETIC_MESSAGE_TYPE_SECURITY_RESPONSE:
    case KINETIC_MESSAGE_TYPE_SECURITY:
    case KINETIC_MESSAGE_TYPE_PEER2PEERPUSH_RESPONSE:
    case KINETIC_MESSAGE_TYPE_PEER2PEERPUSH:
    case KINETIC_MESSAGE_TYPE_NOOP_RESPONSE:
    case KINETIC_MESSAGE_TYPE_NOOP:
    case KINETIC_MESSAGE_TYPE_FLUSHALLDATA_RESPONSE:
    case KINETIC_MESSAGE_TYPE_FLUSHALLDATA:
    case KINETIC_MESSAGE_TYPE_PINOP_RESPONSE:
    case KINETIC_MESSAGE_TYPE_PINOP:
    case KINETIC_MESSAGE_TYPE_MEDIASCAN_RESPONSE:
    case KINETIC_MESSAGE_TYPE_MEDIASCAN:
    case KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE_RESPONSE:
    case KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE:
      return KineticMessageTypeNames[type];
    default:
    case KINETIC_MESSAGE_TYPE_INVALID:
      return KineticMessageTypeNames[0];
    };
}
