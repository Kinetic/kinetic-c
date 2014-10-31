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

static SystemTestFixture Fixture;
static char HmacKeyString[] = "asdfasdf";
static ByteArray HmacKey;
static uint8_t KeyData[1024];
static ByteArray Key;
static ByteBuffer KeyBuffer;
static uint8_t TagData[1024];
static ByteArray Tag;
static ByteBuffer TagBuffer;
static uint8_t VersionData[1024];
static ByteArray Version;
static ByteBuffer VersionBuffer;
static ByteArray TestValue;
static uint8_t ValueData[KINETIC_OBJ_SIZE];
static ByteArray Value;
static ByteBuffer ValueBuffer;

void setUp(void)
{
    SystemTestSetup(&Fixture);

    HmacKey = ByteArray_CreateWithCString(HmacKeyString);
    Fixture.config.hmacKey = HmacKey;

    Key = ByteArray_Create(KeyData, sizeof(KeyData));
    KeyBuffer = ByteBuffer_CreateWithArray(Key);
    ByteBuffer_AppendCString(&KeyBuffer, "DELETE test key");

    Tag = ByteArray_Create(TagData, sizeof(TagData));
    TagBuffer = ByteBuffer_CreateWithArray(Tag);
    ByteBuffer_AppendCString(&TagBuffer, "SomeTagValue");

    Version = ByteArray_Create(VersionData, sizeof(VersionData));
    VersionBuffer = ByteBuffer_CreateWithArray(Version);
    ByteBuffer_AppendCString(&VersionBuffer, "v1.0");

    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah blah blah... etc.");
    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ValueBuffer = ByteBuffer_CreateWithArray(Value);
    ByteBuffer_AppendCString(&ValueBuffer, "lorem ipsum... blah blah blah... etc.");

    ByteBuffer_Append(&Fixture.keyToDelete, Key.data, KeyBuffer.bytesUsed);
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_Delete_should_delete_an_object_from_device(void)
{
    LOG_LOCATION;
    KineticStatus status;

    // Create an object so that we have something to delete
    KineticEntry putEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Put(Fixture.handle, &putEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(Key, putEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, putEntry.tag.array);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putEntry.algorithm);

    // Validate the object exists initially
    KineticEntry getEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Get(Fixture.handle, &getEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.key.array, getEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.tag.array, getEntry.tag.array);
    TEST_ASSERT_EQUAL(putEntry.algorithm, getEntry.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(putEntry.value, getEntry.value);

    // Delete the object
    KineticEntry deleteEntry = {
        .key = KeyBuffer,
    };
    status = KineticClient_Delete(Fixture.handle, &deleteEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, deleteEntry.value.bytesUsed);

    // Validate the object no longer exists
    KineticEntry regetEntryMetadata = {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .metadataOnly = true,
    };
    status = KineticClient_Get(Fixture.handle, &regetEntryMetadata, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    TEST_ASSERT_ByteArray_EMPTY(regetEntryMetadata.value.array);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
