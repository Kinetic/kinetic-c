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
}

void tearDown(void)
{ LOG_LOCATION;
    SystemTestTearDown(&Fixture);
}

void test_GetLog_should_retrieve_capacities_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_CAPACITIES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_utilizations_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_temeratures_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_TEMPERATURES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_configuration_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_CONFIGURATION, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_statistics_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_STATISTICS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_messages_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_MESSAGES, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

void test_GetLog_should_retrieve_limits_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_LIMITS, &Info, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
    TEST_IGNORE_MESSAGE("TODO: Need to implement log decoding for device limits from GetLog!")
}

void test_GetLog_should_retrieve_device_info_from_device(void)
{ LOG_LOCATION;
    Status = KineticClient_GetLog(Fixture.handle, KINETIC_DEVICE_INFO_TYPE_DEVICE, &Info, NULL);
    if (Status == KINETIC_STATUS_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Java simulator currently does NOT support GETLOG 'DEVICE' info attribute!");
    }
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, Status);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
