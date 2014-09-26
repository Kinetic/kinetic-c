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
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static SystemTestFixture Fixture = {
    .config = (KineticSession)
    {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity =  1,
        .nonBlocking = false,
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
    Fixture.config.hmacKey = ByteArray_CreateWithCString("asdfasdf");
    ValueKey = ByteArray_CreateWithCString("DELETE test key");
    Version = ByteArray_CreateWithCString("v1.0");
    NewVersion = ByteArray_CreateWithCString("v2.0");
    Tag = ByteArray_CreateWithCString("SomeTagValue");
    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah... etc...");
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
    KineticEntry putEntry = {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .newVersion = ByteBuffer_CreateWithArray(Version),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
    };
    status = KineticClient_Put(Fixture.handle, &putEntry);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(Version, putEntry.dbVersion.array);
    TEST_ASSERT_ByteArray_NONE(putEntry.newVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, putEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, putEntry.tag.array);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putEntry.algorithm);

    // Validate the object exists initially
    KineticEntry getEntry = {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .value = ByteBuffer_CreateWithArray(ValueIn),
    };
    status = KineticClient_Get(Fixture.handle, &getEntry);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.key.array, getEntry.key.array);
    TEST_ASSERT_ByteArray_NONE(putEntry.newVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.tag.array, getEntry.tag.array);
    TEST_ASSERT_EQUAL(putEntry.algorithm, getEntry.algorithm);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.value.array, getEntry.value.array);

    // Delete the object
    KineticEntry deleteEntry = {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .dbVersion = ByteBuffer_CreateWithArray(Version),
        .algorithm = KINETIC_ALGORITHM_SHA1,
    };
    status = KineticClient_Delete(Fixture.handle, &deleteEntry);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, deleteEntry.value.array.len);

    // Validate the object no longer exists
    KineticEntry regetEntry = {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .value = ByteBuffer_CreateWithArray(ValueIn),
    };
    status = KineticClient_Get(Fixture.handle, &regetEntry);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_DATA_ERROR, status);
    TEST_ASSERT_ByteArray_EMPTY(regetEntry.value.array);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
