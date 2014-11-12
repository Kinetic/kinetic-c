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

#include "kinetic_device_info.h"
#include "kinetic_serial_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>

static size_t sizeof_KineticDeviceInfo_Utilizations(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    size_t count = getLog->n_utilizations;
    size += count * (sizeof(KineticDeviceInfo_Utilization*) + sizeof(KineticDeviceInfo_Utilization));
    for (size_t i = 0; i < count; i++) {
        if (getLog->utilizations[i]->name != NULL) {
            size += strlen(getLog->utilizations[i]->name) + 1;
        }
    }
    return size;
}

static size_t sizeof_KineticDeviceInfo_Temperatures(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_KineticDeviceInfo_Capacity(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_KineticDeviceInfo_Configuration(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_KineticDeviceInfo_Statistics(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_messages(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_KineticDeviceInfo_Limits(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}
static size_t sizeof_KineticDeviceInfo_Device(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    return size;
}

static size_t sizeof_KineticDeviceInfo(const KineticProto_Command_GetLog* getLog)
{
    size_t size = 0;
    size += sizeof(KineticDeviceInfo);
    size += sizeof_KineticDeviceInfo_Utilizations(getLog);
    size += sizeof_KineticDeviceInfo_Temperatures(getLog);
    size += sizeof_KineticDeviceInfo_Capacity(getLog);
    size += sizeof_KineticDeviceInfo_Configuration(getLog);
    size += sizeof_KineticDeviceInfo_Statistics(getLog);
    size += sizeof_messages(getLog);
    size += sizeof_KineticDeviceInfo_Limits(getLog);
    size += sizeof_KineticDeviceInfo_Device(getLog);
    return size;
}

KineticDeviceInfo* KineticDeviceInfo_Create(const KineticProto_Command_GetLog* getLog)
{
    assert(getLog != NULL);

    // Determine the total size to allocate for all available data
    size_t totalSize = sizeof_KineticDeviceInfo(getLog);
    KineticDeviceInfo* info = NULL;

    // Allocate the data
    KineticSerialAllocator allocator = KineticSerialAllocator_Create(totalSize);

    info = (KineticDeviceInfo*)allocator.buffer;

    // Copy data into the nested allocated structure tree

    return info;
}
