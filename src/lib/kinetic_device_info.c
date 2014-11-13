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

#define COPY_VALUE_OPTIONAL(_attr, _dest, _src) { \
    if (_src->has_##_attr) { _dest->_attr = _src->_attr; } }
#define COPY_CSTRING(_attr, _dest, _src, _allocator) \
    if (_src->_attr != NULL) { \
        size_t len = strlen(_src->_attr)+1; \
        _dest->_attr = KineticSerialAllocator_AllocateChunk(_allocator, len); \
        memcpy(_dest->_attr, _src->_attr, len); \
    }
#define COPY_BYTES_OPTIONAL(_attr, _dest, _src, _allocator) \
    if (_src->has_##_attr) { \
        _dest->_attr.data = KineticSerialAllocator_AllocateChunk(_allocator, _src->_attr.len); \
        memcpy(_dest->_attr.data, _src->_attr.data, _src->_attr.len); \
        _dest->_attr.len = _src->_attr.len; \
    }

static void KineticDeviceInfo_ExtractUtilizations(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    info->numUtilizations = getLog->n_utilizations;
    if (info->numUtilizations > 0) {
        info->utilizations = KineticSerialAllocator_AllocateChunk(allocator,
            info->numUtilizations * sizeof(KineticDeviceInfo_Utilization));
        for (size_t i = 0; i < info->numUtilizations; i++) {
            KineticDeviceInfo_Utilization* destUtilization = &(info->utilizations[i]);
            COPY_CSTRING(name,         destUtilization, getLog->utilizations[i], allocator);
            COPY_VALUE_OPTIONAL(value, destUtilization, getLog->utilizations[i]);
        }
    }
}

static void KineticDeviceInfo_ExtractTemperatures(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->n_temperatures > 0) {
        info->numTemperatures = getLog->n_temperatures;
        info->temperatures = KineticSerialAllocator_AllocateChunk(allocator,
            info->numTemperatures * sizeof(KineticDeviceInfo_Temperature));
        for (size_t i = 0; i < info->numTemperatures; i++) {
            KineticDeviceInfo_Temperature* destTemp = &info->temperatures[i];
            COPY_CSTRING(name,  destTemp, getLog->temperatures[i], allocator);
            COPY_VALUE_OPTIONAL(current, destTemp, getLog->temperatures[i]);
            COPY_VALUE_OPTIONAL(minimum, destTemp, getLog->temperatures[i]);
            COPY_VALUE_OPTIONAL(maximum, destTemp, getLog->temperatures[i]);
            COPY_VALUE_OPTIONAL(target,  destTemp, getLog->temperatures[i]);
        }
    }
}

static void KineticDeviceInfo_ExtractCapacity(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->capacity != NULL) {
        info->capacity = KineticSerialAllocator_AllocateChunk(allocator, sizeof(KineticDeviceInfo_Capacity));
        COPY_VALUE_OPTIONAL(nominalCapacityInBytes, info->capacity, getLog->capacity);
        COPY_VALUE_OPTIONAL(portionFull,            info->capacity, getLog->capacity);
    }
}

static void KineticDeviceInfo_ExtractConfiguration(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->configuration != NULL) {
        info->configuration = KineticSerialAllocator_AllocateChunk(allocator,
            sizeof(KineticDeviceInfo_Configuration));
        COPY_CSTRING(vendor,                  info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(model,                   info->configuration, getLog->configuration, allocator);
        COPY_BYTES_OPTIONAL(serialNumber,     info->configuration, getLog->configuration, allocator);
        COPY_BYTES_OPTIONAL(worldWideName,    info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(version,                 info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(compilationDate,         info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(sourceHash,              info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(protocolVersion,         info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(protocolCompilationDate, info->configuration, getLog->configuration, allocator);
        COPY_CSTRING(protocolSourceHash,      info->configuration, getLog->configuration, allocator);
        if (getLog->configuration->n_interface > 0) {
            info->configuration->interfaces = KineticSerialAllocator_AllocateChunk(allocator,
                sizeof(KineticDeviceInfo_Interface) * getLog->configuration->n_interface);
            info->configuration->numInterfaces = getLog->configuration->n_interface;
            for (size_t i = 0; i < getLog->configuration->n_interface; i++) {
                KineticDeviceInfo_Interface* destIface = &info->configuration->interfaces[i];
                KineticProto_Command_GetLog_Configuration_Interface* srcIface = getLog->configuration->interface[i];
                COPY_CSTRING(name,               destIface, srcIface, allocator);
                COPY_BYTES_OPTIONAL(MAC,         destIface, srcIface, allocator);
                COPY_BYTES_OPTIONAL(ipv4Address, destIface, srcIface, allocator);
                COPY_BYTES_OPTIONAL(ipv6Address, destIface, srcIface, allocator);
            }
        }
        COPY_VALUE_OPTIONAL(port,    info->configuration, getLog->configuration);
        COPY_VALUE_OPTIONAL(tlsPort, info->configuration, getLog->configuration);
    }
}

static void KineticDeviceInfo_ExtractStatistics(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->statistics != NULL && getLog->n_statistics > 0) {
        info->numStatistics = getLog->n_statistics;
        info->statistics = KineticSerialAllocator_AllocateChunk(allocator,
            sizeof(KineticDeviceInfo_Statistics) * getLog->n_statistics);
        for (size_t i = 0; i < getLog->n_statistics; i++) {
            KineticDeviceInfo_Statistics* destStats = &info->statistics[i];
            KineticProto_Command_GetLog_Statistics* srcStats = getLog->statistics[i];
            destStats->messageType = KineticProto_Command_MessageType_to_KineticMessageType(srcStats->messageType);
            COPY_VALUE_OPTIONAL(count, destStats, srcStats);
            COPY_VALUE_OPTIONAL(bytes, destStats, srcStats);
        }
    }
}

static void KineticDeviceInfo_ExtractMessages(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    COPY_BYTES_OPTIONAL(messages, info, getLog, allocator);
}

static void KineticDeviceInfo_ExtractLimits(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->limits != NULL) {
        info->limits = KineticSerialAllocator_AllocateChunk(allocator, sizeof(KineticDeviceInfo_Limits));
        info->limits->maxKeySize = getLog->limits->maxKeySize;
        info->limits->maxValueSize = getLog->limits->maxValueSize;
        info->limits->maxVersionSize = getLog->limits->maxVersionSize;
        info->limits->maxTagSize = getLog->limits->maxTagSize;
        info->limits->maxConnections = getLog->limits->maxConnections;
        info->limits->maxOutstandingReadRequests = getLog->limits->maxOutstandingReadRequests;
        info->limits->maxOutstandingWriteRequests = getLog->limits->maxOutstandingWriteRequests;
        info->limits->maxMessageSize = getLog->limits->maxMessageSize;
        info->limits->maxKeyRangeCount = getLog->limits->maxKeyRangeCount;
        info->limits->maxIdentityCount = getLog->limits->maxIdentityCount;
        info->limits->maxPinSize = getLog->limits->maxPinSize;
    }
}

static void KineticDeviceInfo_ExtractDevice(
    KineticDeviceInfo* info,
    const KineticProto_Command_GetLog* getLog,
    KineticSerialAllocator* allocator)
{
    if (getLog->device != NULL) {
        info->device = KineticSerialAllocator_AllocateChunk(allocator, sizeof(KineticDeviceInfo_Device));
        COPY_BYTES_OPTIONAL(name, info->device, getLog->device, allocator);
    }
}

KineticDeviceInfo* KineticDeviceInfo_Create(const KineticProto_Command_GetLog* getLog)
{
    assert(getLog != NULL);

    // Allocate the data
    KineticSerialAllocator allocator = KineticSerialAllocator_Create(KINETIC_DEVICE_INFO_SCRATCH_BUF_LEN);

    // Copy data into the nested allocated structure tree
    KineticDeviceInfo* info = KineticSerialAllocator_AllocateChunk(&allocator, sizeof(KineticDeviceInfo));
    KineticDeviceInfo_ExtractUtilizations(info, getLog, &allocator);
    KineticDeviceInfo_ExtractCapacity(info, getLog, &allocator);
    KineticDeviceInfo_ExtractTemperatures(info, getLog, &allocator);
    KineticDeviceInfo_ExtractConfiguration(info, getLog, &allocator);
    KineticDeviceInfo_ExtractStatistics(info, getLog, &allocator);
    KineticDeviceInfo_ExtractMessages(info, getLog, &allocator);
    KineticDeviceInfo_ExtractLimits(info, getLog, &allocator);
    KineticDeviceInfo_ExtractDevice(info, getLog, &allocator);

    // Trim the buffer prior to returning in order to free up unused memory
    info->totalLength = allocator.used;
    info->totalLength = KineticSerialAllocator_TrimBuffer(&allocator);
    info = KineticSerialAllocator_GetBuffer(&allocator);
    LOGF2("Created KineticDeviceInfo @ 0x%0llX w/ length=%zu", info, info->totalLength);
    return info;
}
