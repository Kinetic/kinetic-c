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
    "NO_PDUS_AVAILABLE",
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
    "MISSING_PIN",
    "SSL_REQUIRED",
    "DEVICE_LOCKED",
    "ACL_ERROR",
    "NOT_AUTHORIZED",
    "INVALID_FILE",
    "REQUEST_REJECTED",
    "DEVICE_NAME_REQUIRED",
    "INVALID_LOG_TYPE",
    "HMAC_FAILURE",
    "SESSION_TERMINATED",
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
    "<UNKNOWN>",
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
    "MEDIAOPTIMIZE",
};

const char* KineticMessageType_GetName(KineticMessageType type)
{
    switch(type) {
    case KINETIC_MESSAGE_TYPE_GET_RESPONSE:
	return "GET_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GET:
	return "GET";
	break;
    case KINETIC_MESSAGE_TYPE_PUT_RESPONSE:
	return "PUT_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_PUT:
	return "PUT";
	break;
    case KINETIC_MESSAGE_TYPE_DELETE_RESPONSE:
	return  "DELETE_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_DELETE:
	return "DELETE";
	break;
    case KINETIC_MESSAGE_TYPE_GETNEXT_RESPONSE:
	return "GETNEXT_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GETNEXT:
	return  "GETNEXT";
	break;
    case KINETIC_MESSAGE_TYPE_GETPREVIOUS_RESPONSE:
	return  "GETPREVIOUS_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GETPREVIOUS:
	return "GETPREVIOUS";
	break;
    case KINETIC_MESSAGE_TYPE_GETKEYRANGE_RESPONSE:
	return "GETKEYRANGE_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GETKEYRANGE:
	return "GETKEYRANGE";
	break;
    case KINETIC_MESSAGE_TYPE_GETVERSION_RESPONSE:
	return "GETVERSION_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GETVERSION:
	return "GETVERSION";
	break;
    case KINETIC_MESSAGE_TYPE_SETUP_RESPONSE:
	return "SETUP_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_SETUP:
	return "SETUP";
	break;
    case KINETIC_MESSAGE_TYPE_GETLOG_RESPONSE:
	return "GETLOG_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_GETLOG:
	return "GETLOG";
	break;
    case KINETIC_MESSAGE_TYPE_SECURITY_RESPONSE:
	return "SECURITY_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_SECURITY:
	return "SECURITY";
	break;
    case KINETIC_MESSAGE_TYPE_PEER2PEERPUSH_RESPONSE:
	return "PEER2PEERPUSH_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_PEER2PEERPUSH:
	return "PEER2PEERPUSH";
	break;
    case KINETIC_MESSAGE_TYPE_NOOP_RESPONSE:
	return "NOOP_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_NOOP:
	return "NOOP";
	break;
    case KINETIC_MESSAGE_TYPE_FLUSHALLDATA_RESPONSE:
	return "FLUSHALLDATA_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_FLUSHALLDATA:
	return "FLUSHALLDATA";
	break;

    case KINETIC_MESSAGE_TYPE_PINOP_RESPONSE:
	return "PINOP_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_PINOP:
	return "PINOP";
	break;
    case KINETIC_MESSAGE_TYPE_MEDIASCAN_RESPONSE:
	return "MEDIASCAN_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_MEDIASCAN:
	return "MEDIASCAN";
	break;
    case KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE_RESPONSE:
	return "MEDIAOPTIMIZE_RESPONSE";
	break;
    case KINETIC_MESSAGE_TYPE_MEDIAOPTIMIZE:
	return "MEDIAOPTIMIZE";
	break;
    default:
    case KINETIC_MESSAGE_TYPE_INVALID:
      return KineticMessageTypeNames[0];
    };
}
