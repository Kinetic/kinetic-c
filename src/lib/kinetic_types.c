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
    "HMAC_EMPTY",
    "NO_PDUS_AVAVILABLE",
    "DEVICE_BUSY",
    "CONNECTION_ERROR",
    "INVALID_REQUEST",
    "OPERATION_INVALID",
    "OPERATION_FAILED",
    "VERSION_FAILURE",
    "DATA_ERROR",
    "BUFFER_OVERRUN",
    "MEMORY_ERROR",
    "SOCKET_TIMEOUT",
    "SOCKET_ERROR",
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
