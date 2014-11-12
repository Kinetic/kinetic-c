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

// static size_t sizeof_KineticDeviceInfo_Utilizations(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     size += getLog->n_utilizations * sizeof(KineticDeviceInfo_Utilization);
//     for (size_t i = 0; i < getLog->n_utilizations; i++) {
//         if (getLog->utilizations[i]->name != NULL) {
//             size += strlen(getLog->utilizations[i]->name) + 1;
//         }
//     }
//     return size;
// }

// static size_t sizeof_KineticDeviceInfo_Temperatures(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_KineticDeviceInfo_Capacity(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_KineticDeviceInfo_Configuration(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_KineticDeviceInfo_Statistics(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_messages(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_KineticDeviceInfo_Limits(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }
// static size_t sizeof_KineticDeviceInfo_Device(const KineticProto_Command_GetLog* getLog)
// {
//     size_t size = 0;
//     return size;
// }

KineticDeviceInfo* KineticDeviceInfo_Create(const KineticProto_Command_GetLog* getLog)
{
    assert(getLog != NULL);

    // Allocate the data
    KineticSerialAllocator allocator = KineticSerialAllocator_Create(KINETIC_DEVICE_INFO_SCRATCH_BUF_LEN);

    // Copy data into the nested allocated structure tree
    KineticDeviceInfo* info = KineticSerialAllocator_AllocateChunk(&allocator, sizeof(KineticDeviceInfo));
    info->numUtilizations = getLog->n_utilizations;
    if (info->numUtilizations > 0) {
        info->utilizations = KineticSerialAllocator_AllocateChunk(&allocator,
            info->numUtilizations * sizeof(KineticDeviceInfo_Utilization));
        for (size_t i = 0; i < info->numUtilizations; i++) {
            // Copy name string
            const char* protoName = getLog->utilizations[i]->name;
            if (protoName != NULL) {
                size_t len = strlen(protoName) + 1;
                info->utilizations[i].name =
                    KineticSerialAllocator_AllocateChunk(&allocator, len);
                memcpy(info->utilizations[i].name, protoName, len);
            }
            // Copy value
            if (getLog->utilizations[i]->has_value) {
                info->utilizations[i].value = getLog->utilizations[i]->value;
            }
        }
    }
    assert(info == KineticSerialAllocator_GetBuffer(&allocator));
    info->totalLength = allocator.used;

    // Trim the buffer prior to returning in order to free up unused memory
    info->totalLength = KineticSerialAllocator_TrimBuffer(&allocator);
    info = KineticSerialAllocator_GetBuffer(&allocator);
    LOGF2("Created KineticDeviceInfo @ 0x%0llX w/ length=%zu", info, info->totalLength);
    return info;
}
