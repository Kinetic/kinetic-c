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
#include "system_test_fixture.h"
#include "kinetic_admin_client.h"
#include "kinetic_device_info.h"
#include <stdlib.h>

static KineticStatus Status;
static KineticLogInfo* Info;

void setUp(void)
{
    SystemTestSetup(1, true);
    Info = NULL;
}

void tearDown(void)
{
    if (Info != NULL) {
        KineticLogInfo_Free(Info);
        Info = NULL;
    };
    SystemTestShutDown();
}

void test_GetLog_should_retrieve_utilizations_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info);
    TEST_ASSERT_NOT_NULL(Info->utilizations);
    TEST_ASSERT_TRUE(Info->numUtilizations >= 4);
    for (size_t i = 0; i < Info->numUtilizations; i++) {
        TEST_ASSERT_NOT_NULL(Info->utilizations[i].name);
        KineticLogInfo_Utilization* utilization = &Info->utilizations[i];
        LOGF0("info->utilizations[%zu]: %s = %.3f", i, utilization->name, utilization->value);
    }
}

void test_GetLog_should_retrieve_capacity_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_CAPACITIES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->capacity);

    LOGF0("info->capacity['nominalCapacityInBytes']: %.llu", Info->capacity->nominalCapacityInBytes);
    LOGF0("info->capacity['portionFull']: %.3f", Info->capacity->portionFull);
}

void test_GetLog_should_retrieve_temeratures_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_TEMPERATURES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_TRUE(Info->numTemperatures >= 2);

    for (size_t i = 0; i < Info->numTemperatures; i++) {
        TEST_ASSERT_NOT_NULL(Info->temperatures[i].name);
        LOGF0("info->temperatures['%s']: current=%.1f, minimum=%.1f, maximum=%.1f, target=%.1f",
            Info->temperatures[i].name,
            Info->temperatures[i].current, Info->temperatures[i].minimum,
            Info->temperatures[i].maximum, Info->temperatures[i].target);
    }
}

void test_GetLog_should_retrieve_configuration_from_device(void)
{
    char buf[1024];

    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_CONFIGURATION, &Info, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info);
    TEST_ASSERT_NOT_NULL(Info->configuration);

    LOG0("info->configuration:");
    LOGF0("  vendor: %s", Info->configuration->vendor);
    LOGF0("  model: %s", Info->configuration->model);
    BYTES_TO_CSTRING(buf, Info->configuration->serialNumber, 0, Info->configuration->serialNumber.len);
    LOGF0("  serialNumber: %s", buf);
    BYTES_TO_CSTRING(buf, Info->configuration->worldWideName, 0, Info->configuration->worldWideName.len);
    LOGF0("  worldWideName: %s", buf);
    LOGF0("  version: %s", Info->configuration->version);
    LOGF0("  compilationDate: %s", Info->configuration->compilationDate);
    LOGF0("  sourceHash: %s", Info->configuration->sourceHash);
    LOGF0("  protocolVersion: %s", Info->configuration->protocolVersion);
    LOGF0("  protocolCompilationDate: %s", Info->configuration->protocolCompilationDate);
    LOGF0("  protocolSourceHash: %s", Info->configuration->protocolSourceHash);
    for (size_t i = 0; i < Info->configuration->numInterfaces; i++) {
        KineticLogInfo_Interface* iface = &Info->configuration->interfaces[i];
        TEST_ASSERT_NOT_NULL(iface->name);
        LOGF0("  interface: %s", iface->name);
        BYTES_TO_CSTRING(buf, iface->MAC, 0, iface->MAC.len);
        LOGF0("    MAC: %s", buf);
        BYTES_TO_CSTRING(buf, iface->ipv4Address, 0, iface->ipv4Address.len);
        LOGF0("    ipv4Address: %s", buf);
        BYTES_TO_CSTRING(buf, iface->ipv6Address, 0, iface->ipv6Address.len);
        LOGF0("    ipv6Address: %s", buf);
    }
    LOGF0("  port: %d", Info->configuration->port);
    LOGF0("  tlsPort: %d", Info->configuration->tlsPort);
}

void test_GetLog_should_retrieve_statistics_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_STATISTICS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->statistics);
    TEST_ASSERT_TRUE(Info->numStatistics > 0);

    LOGF0("Device Statistics (count=%zu)", Info->numStatistics);
    for (size_t i = 0; i < Info->numStatistics; i++) {
        KineticLogInfo_Statistics* stats = &Info->statistics[i];
        LOGF0("  %s: count=%llu, bytes=%llu",
            KineticMessageType_GetName(stats->messageType),
            stats->count, stats->bytes);
    }
}

uint8_t* Buffer[1024 * 1024 * 10];

void test_GetLog_should_retrieve_messages_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_MESSAGES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->messages.data);
    TEST_ASSERT_TRUE(Info->messages.len > 0);

    LOGF0("Device Message Log Contents (len=%zu):", Info->messages.len);
    BYTES_TO_CSTRING(Buffer, Info->messages, 0, Info->messages.len);
    LOGF0("  hex:   %s", Buffer);
    char* msgs = calloc(1, Info->messages.len + 1);
    TEST_ASSERT_NOT_NULL(msgs);
    memcpy(msgs, Info->messages.data, Info->messages.len);
    LOGF0("  ascii: %s", msgs);
    free(msgs);
}

void test_GetLog_should_retrieve_limits_from_device(void)
{
    Status = KineticAdminClient_GetLog(Fixture.session, KINETIC_DEVICE_INFO_TYPE_LIMITS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->limits);

    LOG0("Device Limits:");
    LOGF0("  maxKeySize: %u",                  Info->limits->maxKeySize);
    LOGF0("  maxValueSize: %u",                Info->limits->maxValueSize);
    LOGF0("  maxVersionSize: %u",              Info->limits->maxVersionSize);
    LOGF0("  maxTagSize: %u",                  Info->limits->maxTagSize);
    LOGF0("  maxConnections: %u",              Info->limits->maxConnections);
    LOGF0("  maxOutstandingReadRequests: %u",  Info->limits->maxOutstandingReadRequests);
    LOGF0("  maxOutstandingWriteRequests: %u", Info->limits->maxOutstandingWriteRequests);
    LOGF0("  maxMessageSize: %u",              Info->limits->maxMessageSize);
    LOGF0("  maxKeyRangeCount: %u",            Info->limits->maxKeyRangeCount);
    LOGF0("  maxIdentityCount: %u",            Info->limits->maxIdentityCount);
    LOGF0("  maxPinSize: %u",                  Info->limits->maxPinSize);
}

void test_GetDeviceLog_should_retrieve_device_info_from_device(void)
{
    char nameData[] = "com.Seagate";
    ByteArray name = {
        .data = (uint8_t*)nameData,
        .len = strlen(nameData),
    };

    Status = KineticAdminClient_GetDeviceSpecificLog(Fixture.session, name, &Info, NULL);

    if (Status == KINETIC_STATUS_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Simulator/Drive returned NOT_FOUND for GetLog request for Device info.");
    }

    TEST_ASSERT_NOT_NULL(Info);
    TEST_ASSERT_NOT_NULL(Info->device);
    TEST_ASSERT_NOT_NULL(Info->device->name.data);
    TEST_ASSERT_TRUE(Info->device->name.len > 0);
    LOG0("Device Data:");
    char* dev = calloc(1, Info->device->name.len + 1);
    TEST_ASSERT_NOT_NULL(dev);
    memcpy(dev, Info->device->name.data, Info->device->name.len);
    LOGF0("  ascii: %s", dev);
    free(dev);
}
