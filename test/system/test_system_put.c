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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
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

#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static SystemTestFixture Fixture = {
    .config = (KineticSession) {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity =  1,
        .nonBlocking = false,
        .hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("asdfasdf"),
    }
};
static ByteArray valueKey;
static ByteArray tag;
static ByteArray testValue;

void setUp(void)
{
    SystemTestSetup(&Fixture);
    valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    testValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah... etc...");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

// -----------------------------------------------------------------------------
// Put Command - Write a blob of data to a Kinetic Device
//
// Inspected Request: (m/d/y)
// -----------------------------------------------------------------------------
//
//  TBD!
//
void test_Put_should_create_new_object_on_device(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = valueKey,
        .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = testValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
}

void test_Put_should_create_new_object_on_device_again(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927_0"),
        .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = testValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
}

#if 0
void test_Put_should_update_object_data_on_device(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = valueKey,
        .dbVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = testValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = valueKey,
        .dbVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v2.0"),
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = testValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    // Fixture.instance.testIgnored = true;

    // if (status == KINETIC_STATUS_VERSION_FAILURE)
    // {
    //     TEST_IGNORE_MESSAGE(
    //         "Java simulator is responding with VERSION_MISMATCH(8) if algorithm "
    //         "un-specified on initial PUT and subsequent request updates dbVersion!");
    // }

    // TEST_ASSERT_EQUAL_KINETIC_STATUS(
    //     KINETIC_STATUS_SUCCESS, status);
}
#endif

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
