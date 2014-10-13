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

#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "zlog/zlog.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static SystemTestFixture Fixture;
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
static const char strKey[] = "GET system test blob";

void setUp(void)
{ LOG_LOCATION;
    SystemTestSetup(&Fixture);

    Key = ByteArray_Create(KeyData, sizeof(KeyData));
    KeyBuffer = ByteBuffer_CreateWithArray(Key);
    ByteBuffer_AppendCString(&KeyBuffer, strKey);

    Tag = ByteArray_Create(TagData, sizeof(TagData));
    TagBuffer = ByteBuffer_CreateWithArray(Tag);
    ByteBuffer_AppendCString(&TagBuffer, "SomeTagValue");

    Version = ByteArray_Create(VersionData, sizeof(VersionData));
    VersionBuffer = ByteBuffer_CreateWithArray(Version);
    ByteBuffer_AppendCString(&VersionBuffer, "v1.0");

    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah blah blah... etc.");
    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ValueBuffer = ByteBuffer_CreateWithArray(Value);
    ByteBuffer_AppendArray(&ValueBuffer, TestValue);

    ByteBuffer_Append(&Fixture.keyToDelete, Key.data, KeyBuffer.bytesUsed);

    // Setup to write some test data
    KineticEntry putEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .newVersion = VersionBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &putEntry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NOT_NULL(putEntry.dbVersion.array.data);
    TEST_ASSERT_EQUAL(strlen("v1.0"), putEntry.dbVersion.bytesUsed);
    TEST_ASSERT_EQUAL(0, putEntry.newVersion.bytesUsed);
    TEST_ASSERT_NULL(putEntry.newVersion.array.data);
    TEST_ASSERT_EQUAL(0, putEntry.newVersion.array.len);
    TEST_ASSERT_EQUAL(0, putEntry.newVersion.bytesUsed);
    TEST_ASSERT_EQUAL_ByteArray(Key, putEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, putEntry.tag.array);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putEntry.algorithm);

    Fixture.expectedSequence++;
}

void tearDown(void)
{ LOG_LOCATION;
    SystemTestTearDown(&Fixture);
}

void test_Get_should_retrieve_object_and_metadata_from_device(void)
{ LOG_LOCATION;
    uint8_t dbVersionData[1024];
    memset(Value.data, 0, Value.len);
    ByteBuffer keyBuffer = ByteBuffer_CreateWithArray(Key);
    keyBuffer.bytesUsed = strlen(strKey);
    KineticEntry getEntry = {
        .key = keyBuffer,
        .dbVersion = ByteBuffer_Create(dbVersionData, sizeof(dbVersionData), 0),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .value = ByteBuffer_CreateWithArray(Value),
    };

    KineticStatus status = KineticClient_Get(Fixture.handle, &getEntry);

    ByteBuffer_AppendArray(&Fixture.keyToDelete, Key);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    ByteBuffer expectedVer = ByteBuffer_Create(Version.data, Version.len, strlen("v1.0"));
    TEST_ASSERT_EQUAL_ByteBuffer(expectedVer, getEntry.dbVersion);
    TEST_ASSERT_ByteArray_NONE(getEntry.newVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Key, getEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, getEntry.tag.array);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
    TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(TestValue, getEntry.value);
}

// void test_Get_should_retrieve_object_and_metadata_from_device_again(void)
// { LOG_LOCATION;
//     sleep(1);

//     KineticEntry entry = {
//         .key = ByteBuffer_CreateWithArray(ValueKey),
//         .value = ByteBuffer_CreateWithArray(Value),
//     };

//     KineticStatus status = KineticClient_Get(Fixture.handle, &entry);

//     TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
//     // TEST_ASSERT_EQUAL_ByteArray(TestValue, metadata.value);
// }

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
