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
#include "kinetic_logger.h"
#include <stdlib.h>
#include <string.h>

/* Using copy_str instead of strdup since it's not necessarily available. */
static char *copy_str(const char *s) {
    if (s == NULL) { return NULL; }

    int len = strlen(s);
    char *res = calloc(len + 1, sizeof(char));
    if (res) {
        memcpy(res, s, len);
        res[len] = '\0';
    }

    return res;
}

/* Copy a byte array. Returns one with res.data == NULL on alloc fail. */
static ByteArray copy_to_byte_array(uint8_t *data, size_t length) {
    ByteArray res = { .len = length, };
    res.data = calloc(res.len, sizeof(uint8_t));
    if (res.data) {
        memcpy(res.data, data, res.len);
    }
    return res;
}

static void free_byte_array(ByteArray ba) {
    free(ba.data);
}

static KineticLogInfo_Utilization* KineticLogInfo_GetUtilizations(
    const KineticProto_Command_GetLog* getLog,
    size_t *numUtilizations)
{
    *numUtilizations = 0;
    size_t num_util = getLog->n_utilizations;

    KineticLogInfo_Utilization * util = calloc(num_util, sizeof(*util));

    if (util) {
        for (size_t i = 0; i < num_util; i++) {
            util[i].name = copy_str(getLog->utilizations[i]->name);
            if (util[i].name == NULL) {
                for (size_t j = 0; j < i; j++) { free(util[j].name); }
                free(util);
                util = NULL;
                break;
            }
            util[i].value = getLog->utilizations[i]->value;
        }

        *numUtilizations = num_util;
    }
    return util;
}

static KineticLogInfo_Temperature *KineticLogInfo_GetTemperatures(
    const KineticProto_Command_GetLog* getLog,
    size_t *numTemperatures)
{
    size_t num_temp = getLog->n_temperatures;
    *numTemperatures = 0;
    KineticLogInfo_Temperature *temp = calloc(num_temp, sizeof(*temp));
    if (temp) {
        for (size_t i = 0; i < num_temp; i++) {
            temp[i].name = copy_str(getLog->temperatures[i]->name);
            temp[i].current = getLog->temperatures[i]->current;
            temp[i].minimum = getLog->temperatures[i]->minimum;
            temp[i].maximum = getLog->temperatures[i]->maximum;
            temp[i].target = getLog->temperatures[i]->target;
        }

        *numTemperatures = num_temp;
    }
    return temp;
}

static KineticLogInfo_Capacity *KineticLogInfo_GetCapacity(
    const KineticProto_Command_GetLog* getLog)
{
    KineticLogInfo_Capacity *cap = calloc(1, sizeof(*cap));
    if (cap && getLog->capacity) {
        cap->nominalCapacityInBytes = getLog->capacity->nominalCapacityInBytes;
        cap->portionFull = getLog->capacity->portionFull;
    }
    return cap;
}

static KineticLogInfo_Configuration * KineticLogInfo_GetConfiguration(
    const KineticProto_Command_GetLog* getLog)
{
    KineticProto_Command_GetLog_Configuration const *gcfg = getLog->configuration;

    KineticLogInfo_Configuration *cfg = calloc(1, sizeof(*cfg));
    if (cfg) {
        if (gcfg->has_serialNumber) {
            cfg->serialNumber = (ByteArray){0, 0};
        }
        if (gcfg->has_worldWideName) {
            cfg->worldWideName = (ByteArray){0, 0};
        }

        cfg->vendor = copy_str(gcfg->vendor);
        if (cfg->vendor == NULL) { goto cleanup; }
        cfg->model = copy_str(gcfg->model);
        if (cfg->model == NULL) { goto cleanup; }
        cfg->version = copy_str(gcfg->version);
        if (cfg->version == NULL) { goto cleanup; }
        cfg->compilationDate = copy_str(gcfg->compilationDate);
        if (cfg->compilationDate == NULL) { goto cleanup; }
        cfg->sourceHash = copy_str(gcfg->sourceHash);
        if (cfg->sourceHash == NULL) { goto cleanup; }
        cfg->protocolVersion = copy_str(gcfg->protocolVersion);
        if (cfg->protocolVersion == NULL) { goto cleanup; }
        cfg->protocolCompilationDate = copy_str(gcfg->protocolCompilationDate);
        if (cfg->protocolCompilationDate == NULL) { goto cleanup; }
        cfg->protocolSourceHash = copy_str(gcfg->protocolSourceHash);
        if (cfg->protocolSourceHash == NULL) { goto cleanup; }

        cfg->numInterfaces = gcfg->n_interface;
        cfg->interfaces = calloc(cfg->numInterfaces, sizeof(*cfg->interfaces));
        if (cfg->interfaces == NULL) { goto cleanup; }
        for (size_t i = 0; i < cfg->numInterfaces; i++) {
            KineticLogInfo_Interface *inf = &cfg->interfaces[i];
            inf->name = copy_str(gcfg->interface[i]->name);
            if (inf->name == NULL) { goto cleanup; }

            if (gcfg->interface[i]->has_MAC) {
                inf->MAC = copy_to_byte_array((uint8_t *)gcfg->interface[i]->MAC.data,
                    gcfg->interface[i]->MAC.len);
                if (inf->MAC.data == NULL) { goto cleanup; }
            }

            if (gcfg->interface[i]->has_ipv4Address) {
                inf->ipv4Address = copy_to_byte_array(gcfg->interface[i]->ipv4Address.data,
                    gcfg->interface[i]->ipv4Address.len);
                if (inf->ipv4Address.data == NULL) { goto cleanup; }
            }

            if (gcfg->interface[i]->has_ipv6Address) {
                inf->ipv6Address = copy_to_byte_array(gcfg->interface[i]->ipv6Address.data,
                    gcfg->interface[i]->ipv6Address.len);
                if (inf->ipv6Address.data == NULL) { goto cleanup; }
            }
        }
    }
    return cfg;
cleanup:
    if (cfg->vendor) { free(cfg->vendor); }
    if (cfg->model) { free(cfg->model); }
    if (cfg->version) { free(cfg->version); }
    if (cfg->compilationDate) { free(cfg->compilationDate); }
    if (cfg->sourceHash) { free(cfg->sourceHash); }
    if (cfg->protocolVersion) { free(cfg->protocolVersion); }
    if (cfg->protocolCompilationDate) { free(cfg->protocolCompilationDate); }
    if (cfg->protocolSourceHash) { free(cfg->protocolSourceHash); }

    if (cfg->interfaces) {
        for (size_t i = 0; i < cfg->numInterfaces; i++) {
            if (cfg->interfaces[i].name) { free(cfg->interfaces[i].name); }
            free_byte_array(cfg->interfaces[i].MAC);
            free_byte_array(cfg->interfaces[i].ipv4Address);
            free_byte_array(cfg->interfaces[i].ipv6Address);
        }
        free(cfg->interfaces);
    }

    free(cfg);
    return NULL;
}

static KineticLogInfo_Statistics *KineticLogInfo_GetStatistics(
    const KineticProto_Command_GetLog* getLog,
    size_t *numStatistics)
{
    size_t num_stats = getLog->n_statistics;
    KineticLogInfo_Statistics *stats = calloc(num_stats, sizeof(*stats));
    *numStatistics = 0;
    if (stats) {
        for (size_t i = 0; i < num_stats; i++) {
            stats[i].messageType = getLog->statistics[i]->messageType;
            if (getLog->statistics[i]->has_count) {
                stats[i].count = getLog->statistics[i]->count;
            }
            if (getLog->statistics[i]->has_bytes) {
                stats[i].bytes = getLog->statistics[i]->bytes;
            }
        }
        *numStatistics = num_stats;
    }
    return stats;
}

static ByteArray KineticLogInfo_GetMessages(
    const KineticProto_Command_GetLog* getLog)
{
    return copy_to_byte_array(getLog->messages.data, getLog->messages.len);
    //COPY_BYTES_OPTIONAL(messages, info, getLog, allocator);
}

static KineticLogInfo_Limits * KineticLogInfo_GetLimits(
    const KineticProto_Command_GetLog* getLog)
{
    KineticLogInfo_Limits * limits = calloc(1, sizeof(*limits));
    if (limits) {
        limits->maxKeySize = getLog->limits->maxKeySize;
        limits->maxValueSize = getLog->limits->maxValueSize;
        limits->maxVersionSize = getLog->limits->maxVersionSize;
        limits->maxTagSize = getLog->limits->maxTagSize;
        limits->maxConnections = getLog->limits->maxConnections;
        limits->maxOutstandingReadRequests = getLog->limits->maxOutstandingReadRequests;
        limits->maxOutstandingWriteRequests = getLog->limits->maxOutstandingWriteRequests;
        limits->maxMessageSize = getLog->limits->maxMessageSize;
        limits->maxKeyRangeCount = getLog->limits->maxKeyRangeCount;
        limits->maxIdentityCount = getLog->limits->maxIdentityCount;
        limits->maxPinSize = getLog->limits->maxPinSize;
    }
    return limits;
}

static KineticLogInfo_Device * KineticLogInfo_GetDevice(
    const KineticProto_Command_GetLog* getLog)
{
    KineticLogInfo_Device *device = calloc(1, sizeof(*device));
    if (device && getLog->device) {
        if (getLog->device->has_name) {
            device->name = copy_to_byte_array(getLog->device->name.data,
                getLog->device->name.len);
        }
    }
    return device;
}

KineticLogInfo* KineticLogInfo_Create(const KineticProto_Command_GetLog* getLog)
{
    KINETIC_ASSERT(getLog != NULL);

    // Copy data into the nested allocated structure tree
    KineticLogInfo* info = calloc(1, sizeof(*info));
    if (info == NULL) { return NULL; }
    memset(info, 0, sizeof(*info));

    KineticLogInfo_Utilization* utilizations = NULL;
    KineticLogInfo_Temperature* temperatures = NULL;
    KineticLogInfo_Capacity* capacity = NULL;
    KineticLogInfo_Configuration* configuration = NULL;
    KineticLogInfo_Statistics* statistics = NULL;
    KineticLogInfo_Limits* limits = NULL;
    KineticLogInfo_Device* device = NULL;    

    utilizations = KineticLogInfo_GetUtilizations(getLog, &info->numUtilizations);
    if (utilizations == NULL) { goto cleanup; }
    capacity = KineticLogInfo_GetCapacity(getLog);
    if (capacity == NULL) { goto cleanup; }
    temperatures = KineticLogInfo_GetTemperatures(getLog, &info->numTemperatures);
    if (temperatures == NULL) { goto cleanup; }

    if (getLog->configuration != NULL) {
        configuration = KineticLogInfo_GetConfiguration(getLog);
        if (configuration == NULL) { goto cleanup; }
    }

    statistics = KineticLogInfo_GetStatistics(getLog, &info->numStatistics);
    if (statistics == NULL) { goto cleanup; }
    ByteArray messages = KineticLogInfo_GetMessages(getLog);
    if (messages.data == NULL) { goto cleanup; }

    if (getLog->limits != NULL) {
        limits = KineticLogInfo_GetLimits(getLog);
        if (limits == NULL) { goto cleanup; }
    }

    device = KineticLogInfo_GetDevice(getLog);
    if (device == NULL) { goto cleanup; }

    info->utilizations = utilizations;
    info->temperatures = temperatures;
    info->capacity = capacity;
    info->configuration = configuration;
    info->statistics = statistics;
    info->limits = limits;
    info->device = device;
    info->messages = messages;

    LOGF2("Created KineticLogInfo @ 0x%0llX", info);
    return info;

cleanup:
    if (info) { free(info); }
    if (utilizations) { free(utilizations); }
    if (temperatures) { free(temperatures); }
    if (capacity) { free(capacity); }
    if (configuration) { free(configuration); }
    if (statistics) { free(statistics); }
    if (limits) { free(limits); }
    if (device) { free(device); }
    return NULL;
}

void KineticLogInfo_Free(KineticLogInfo* kdi) {
    if (kdi) {
        if (kdi->utilizations) {
            for (size_t i = 0; i < kdi->numUtilizations; i++) {
                free(kdi->utilizations[i].name);
            }
            free(kdi->utilizations);
        }
        
        if (kdi->temperatures) {
            for (size_t i = 0; i < kdi->numTemperatures; i++) {
                free(kdi->temperatures[i].name);
            }
            free(kdi->temperatures);
        }
        
        if (kdi->capacity) { free(kdi->capacity); }
        
        if (kdi->configuration) {
            KineticLogInfo_Configuration *cfg = kdi->configuration;

            if (cfg->vendor) { free(cfg->vendor); }
            if (cfg->model) { free(cfg->model); }
            if (cfg->version) { free(cfg->version); }
            if (cfg->compilationDate) { free(cfg->compilationDate); }
            if (cfg->sourceHash) { free(cfg->sourceHash); }
            if (cfg->protocolVersion) { free(cfg->protocolVersion); }
            if (cfg->protocolCompilationDate) { free(cfg->protocolCompilationDate); }
            if (cfg->protocolSourceHash) { free(cfg->protocolSourceHash); }

            if (cfg->serialNumber.data) { free(cfg->serialNumber.data); }
            if (cfg->worldWideName.data) { free(cfg->worldWideName.data); }

            if (cfg->interfaces) {
                for (size_t i = 0; i < cfg->numInterfaces; i++) {
                    KineticLogInfo_Interface *inf = &cfg->interfaces[i];
                    if (inf->name) { free(inf->name); }
                    if (inf->MAC.data) { free(inf->MAC.data); }
                    if (inf->ipv4Address.data) { free(inf->ipv4Address.data); }
                    if (inf->ipv6Address.data) { free(inf->ipv6Address.data); }
                }
                free(cfg->interfaces);
            }
            free(cfg);
        }
        
        if (kdi->statistics) { free(kdi->statistics); }
        if (kdi->limits) { free(kdi->limits); }
        
        if (kdi->device) {
            if (kdi->device->name.data) { free(kdi->device->name.data); }
            free(kdi->device);
        }

        if (kdi->messages.data) { free(kdi->messages.data); }

        free(kdi);
    }
}
