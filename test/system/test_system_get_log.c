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

#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_device_info.h"
#include "kinetic_serial_allocator.h"
#include "kinetic_proto.h"
#include "kinetic_allocator.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

static SystemTestFixture Fixture;
static KineticStatus Status;
static KineticDeviceInfo* Info;

void setUp(void)
{ LOG_LOCATION;
    SystemTestSetup(&Fixture);
    Info = NULL;
}

void tearDown(void)
{ LOG_LOCATION;
    SystemTestTearDown(&Fixture);
    if (Info != NULL) {
        free(Info);
        Info = NULL;
    };
}


void test_GetLog_should_retrieve_utilizations_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info);
    TEST_ASSERT_NOT_NULL(Info->utilizations);
    TEST_ASSERT_EQUAL(4, Info->numUtilizations);
    TEST_ASSERT_NOT_NULL(Info->utilizations[0].name);
    TEST_ASSERT_NOT_NULL(Info->utilizations[1].name);
    TEST_ASSERT_NOT_NULL(Info->utilizations[2].name);
    TEST_ASSERT_NOT_NULL(Info->utilizations[3].name);

    for (size_t i = 0; i < 4; i++) {
        KineticDeviceInfo_Utilization* utilization = &Info->utilizations[i];
        LOGF0("info->utilizations[%zu]: %s = %.3f", i, utilization->name, utilization->value);
    }
}

void test_GetLog_should_retrieve_capacity_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_CAPACITIES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->capacity);

    LOGF0("info->capacity['nominalCapacityInBytes']: %.llu", Info->capacity->nominalCapacityInBytes);
    LOGF0("info->capacity['portionFull']: %.3f", Info->capacity->portionFull);
}

void test_GetLog_should_retrieve_temeratures_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_TEMPERATURES, &Info, NULL);
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
{ LOG_LOCATION;
    char buf[1024];
    
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_CONFIGURATION, &Info, NULL);

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
        KineticDeviceInfo_Interface* iface = &Info->configuration->interfaces[i];
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
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_STATISTICS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->statistics);
    TEST_ASSERT_TRUE(Info->numStatistics > 0);

    LOGF0("Device Statistics (count=%zu)", Info->numStatistics);
    for (size_t i = 0; i < Info->numStatistics; i++) {
        KineticDeviceInfo_Statistics* stats = &Info->statistics[i];
        LOGF0("  %s: count=%llu, bytes=%llu",
            KineticMessageType_GetName(stats->messageType),
            stats->count, stats->bytes);
    }
}

uint8_t* Buffer[1024 * 1024 * 4];

void test_GetLog_should_retrieve_messages_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_MESSAGES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->messages.data);
    TEST_ASSERT_TRUE(Info->messages.len > 0);

    LOGF0("Device Message Log Contents (len=%zu):", Info->messages.len);
    BYTES_TO_CSTRING(Buffer, Info->messages, 0, Info->messages.len);
    LOGF0("  %s", Buffer);
}

void test_GetLog_should_retrieve_limits_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_LIMITS, &Info, NULL);
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

void test_GetLog_should_retrieve_device_info_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_DEVICE, &Info, NULL);
    if (Status == KINETIC_STATUS_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Java simulator currently does NOT support GETLOG 'DEVICE' info attribute!");
    }

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_ASSERT_NOT_NULL(Info->device);
    TEST_ASSERT_NOT_NULL(Info->device->name.data);
    TEST_ASSERT_TRUE(Info->device->name.len > 0);

    LOG0("Device Data:");
    BYTES_TO_CSTRING(Buffer, Info->device->name, 0, Info->device->name.len);
    LOGF0("  name: %s", Buffer);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
