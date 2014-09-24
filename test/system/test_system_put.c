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
static ByteArray ValueKey;
static ByteArray OtherValueKey;
static ByteArray Tag;
static ByteArray TestValue;
static ByteArray Version;
static ByteArray NewVersion;

void setUp(void)
{
    SystemTestSetup(&Fixture);
    ValueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    OtherValueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927_0");
    Tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    TestValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah... etc...");
    Version = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    NewVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v2.0");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}


void test_Put_should_create_new_object_on_device(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = ValueKey,
        .newVersion = Version,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, metadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(metadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, metadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, metadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, metadata.algorithm);
}

void test_Put_should_create_another_new_object_on_device(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = OtherValueKey,
        .newVersion = Version,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, metadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(metadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(OtherValueKey, metadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, metadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, metadata.algorithm);
}

void test_Put_should_update_object_data_on_device(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = ValueKey,
        .dbVersion = Version,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, metadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(metadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, metadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, metadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, metadata.algorithm);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{   LOG(""); LOG_LOCATION;
    KineticKeyValue metadata = {
        .key = ValueKey,
        .dbVersion = Version,
        .newVersion = NewVersion,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &metadata);

    Fixture.testIgnored = true;

    if (status == KINETIC_STATUS_VERSION_FAILURE)
    {
        TEST_IGNORE_MESSAGE(
            "Java simulator is responding with VERSION_MISMATCH(8) if algorithm "
            "un-specified on initial PUT and subsequent request updates dbVersion!");
    }

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(NewVersion, metadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(metadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, metadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, metadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, metadata.algorithm);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
