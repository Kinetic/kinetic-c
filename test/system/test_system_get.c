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
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"
#include "kinetic_allocator.h"

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
static ByteArray Tag;
static ByteArray TestValue;
static ByteArray Version;
static ByteArray Value;
static uint8_t ValueBuffer[PDU_VALUE_MAX_LEN];

static bool TestDataWritten = false;

void setUp(void)
{
    SystemTestSetup(&Fixture);
    ValueKey = BYTE_ARRAY_INIT_FROM_CSTRING("GET system test blob");
    Version = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    Tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeOtherTagValue");
    TestValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah blah blah... etc.");
    Value = (ByteArray){.data = ValueBuffer, .len = sizeof(ValueBuffer)};

    // Setup to write some test data
    KineticKeyValue putMetadata = {
        .key = ValueKey,
        .newVersion = Version,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };

    if (!TestDataWritten)
    {
        KineticStatus status = KineticClient_Put(Fixture.handle, &putMetadata);
        TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
        TEST_ASSERT_EQUAL_ByteArray(Version, putMetadata.dbVersion);
        TEST_ASSERT_ByteArray_NONE(putMetadata.newVersion);
        TEST_ASSERT_EQUAL_ByteArray(ValueKey, putMetadata.key);
        TEST_ASSERT_EQUAL_ByteArray(Tag, putMetadata.tag);
        TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putMetadata.algorithm);

        Fixture.expectedSequence++;
        TestDataWritten = true;
    }
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}


void test_Get_should_retrieve_object_and_metadata_from_device(void)
{
    KineticKeyValue getMetadata = {.key = ValueKey, .value = Value};

    KineticStatus status = KineticClient_Get(Fixture.handle, &getMetadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(Version, getMetadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(getMetadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, getMetadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, getMetadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getMetadata.algorithm);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, getMetadata.value);
}

void test_Get_should_retrieve_object_and_metadata_from_device_again(void)
{
    sleep(1);

    KineticKeyValue metadata = {
        .key = ValueKey,
        .value = Value,
    };

    KineticStatus status = KineticClient_Get(Fixture.handle, &metadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    // TEST_ASSERT_EQUAL_ByteArray(TestValue, metadata.value);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
