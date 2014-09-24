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
static ByteArray Tag;
static ByteArray TestValue;
static ByteArray Version;
static ByteArray NewVersion;
static ByteArray TestValue;
static uint8_t ValueBuffer[PDU_VALUE_MAX_LEN];
static ByteArray ValueIn = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void setUp(void)
{
    SystemTestSetup(&Fixture);
    ValueKey = BYTE_ARRAY_INIT_FROM_CSTRING("DELETE test key");
    Version = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    NewVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v2.0");
    Tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    TestValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah... etc...");
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
void test_Delete_should_delete_an_object_from_device(void)
{
    KineticStatus status;

    // Create an object so that we have something to delete
    KineticKeyValue putMetadata = {
        .key = ValueKey,
        .newVersion = Version,
        .tag = Tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = TestValue,
    };
    status = KineticClient_Put(Fixture.handle, &putMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(Version, putMetadata.dbVersion);
    TEST_ASSERT_ByteArray_NONE(putMetadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, putMetadata.key);
    TEST_ASSERT_EQUAL_ByteArray(Tag, putMetadata.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putMetadata.algorithm);

    // Validate the object exists initially
    KineticKeyValue getMetadata = {
        .key = ValueKey,
        .value = ValueIn,
    };
    status = KineticClient_Get(Fixture.handle, &getMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(putMetadata.key, getMetadata.key);
    TEST_ASSERT_ByteArray_NONE(putMetadata.newVersion);
    TEST_ASSERT_EQUAL_ByteArray(putMetadata.tag, getMetadata.tag);
    TEST_ASSERT_EQUAL(putMetadata.algorithm, getMetadata.algorithm);
    TEST_ASSERT_EQUAL_ByteArray(putMetadata.value, getMetadata.value);

    // Delete the object
    KineticKeyValue deleteMetadata = {
        .key = ValueKey,
        .tag = Tag,
        .dbVersion = Version,
        .algorithm = KINETIC_ALGORITHM_SHA1,
    };
    status = KineticClient_Delete(Fixture.handle, &deleteMetadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, deleteMetadata.value.len);

    // Validate the object no longer exists
    KineticKeyValue regetMetadata = {
        .key = ValueKey,
        .value = ValueIn,
    };
    status = KineticClient_Get(Fixture.handle, &regetMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_DATA_ERROR, status);
    TEST_ASSERT_EQUAL(0, regetMetadata.value.len);
    TEST_ASSERT_ByteArray_EMPTY(regetMetadata.value);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
