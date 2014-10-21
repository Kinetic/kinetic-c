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
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static KineticEntry Entry;

static SystemTestFixture Fixture;
static char HmacKeyString[] = "asdfasdf";
static ByteArray HmacKey;

static uint8_t KeyData[1024];
static ByteArray Key;
static ByteBuffer KeyBuffer;

static uint8_t OtherKeyData[1024];
static ByteArray OtherKey;
static ByteBuffer OtherKeyBuffer;

static uint8_t TagData[1024];
static ByteArray Tag;
static ByteBuffer TagBuffer;

static uint8_t VersionData[1024];
static ByteArray Version;
static ByteBuffer VersionBuffer;

static uint8_t NewVersionData[1024];
static ByteArray NewVersion;
static ByteBuffer NewVersionBuffer;

static uint8_t OtherVersionData[1024];
static ByteArray OtherVersion;
static ByteBuffer OtherVersionBuffer;

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
    ByteBuffer_AppendCString(&KeyBuffer, "PUT test key");

    OtherKey = ByteArray_Create(OtherKeyData, sizeof(OtherKeyData));
    OtherKeyBuffer = ByteBuffer_CreateWithArray(OtherKey);
    ByteBuffer_AppendCString(&OtherKeyBuffer, "Some other PUT test key");

    Tag = ByteArray_Create(TagData, sizeof(TagData));
    TagBuffer = ByteBuffer_CreateWithArray(Tag);
    ByteBuffer_AppendCString(&TagBuffer, "SomeTagValue");

    Version = ByteArray_Create(VersionData, sizeof(VersionData));
    VersionBuffer = ByteBuffer_CreateWithArray(Version);
    ByteBuffer_AppendCString(&VersionBuffer, "v1.0");

    NewVersion = ByteArray_Create(NewVersionData, sizeof(NewVersionData));
    NewVersionBuffer = ByteBuffer_CreateWithArray(NewVersion);
    ByteBuffer_AppendCString(&NewVersionBuffer, "v2.0");

    OtherVersion = ByteArray_Create(OtherVersionData, sizeof(OtherVersionData));
    OtherVersionBuffer = ByteBuffer_CreateWithArray(OtherVersion);
    ByteBuffer_AppendCString(&OtherVersionBuffer, "v3.0");

    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah blah blah... etc.");
    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ValueBuffer = ByteBuffer_CreateWithArray(Value);
    ByteBuffer_AppendCString(&ValueBuffer, "lorem ipsum... blah blah blah... etc.");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_Delete_old_object_if_exists(void)
{
    KineticStatus status;

    ByteBuffer_Reset(&VersionBuffer);
    ByteBuffer_Reset(&TagBuffer);
    ByteBuffer_Reset(&ValueBuffer);
    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
    };
    status = KineticClient_Get(Fixture.handle, &Entry);

    if (status == KINETIC_STATUS_SUCCESS) {
        status = KineticClient_Delete(Fixture.handle, &Entry);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    else
    {
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    }


    ByteBuffer_Reset(&VersionBuffer);
    ByteBuffer_Reset(&TagBuffer);
    ByteBuffer_Reset(&ValueBuffer);
    Entry = (KineticEntry) {
        .key = OtherKeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
    };
    status = KineticClient_Get(Fixture.handle, &Entry);

    if (status == KINETIC_STATUS_SUCCESS) {
        status = KineticClient_Delete(Fixture.handle, &Entry);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    else
    {
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    }
}

void test_Put_should_create_new_object_on_device(void)
{
    LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .newVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_create_another_new_object_on_device(void)
{
    LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = OtherKeyBuffer,
        .newVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(OtherKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{
    LOG_LOCATION;
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendCString(&ValueBuffer, "This is some other random to update the entry with... again!");

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .newVersion = NewVersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_ByteBuffer_EMPTY(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(NewVersionBuffer, Entry.dbVersion);
    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_and_with_FLUSH_sync_mode_enabled(void)
{
    LOG_LOCATION;
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendCString(&ValueBuffer, "OK, this is, yet again, some new and different data!");

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = NewVersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .synchronization = KINETIC_SYNCHRONIZATION_FLUSH,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(NewVersion, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_and_with_WRITETHROUGH_sync_mode_enabled(void)
{
    LOG_LOCATION;
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendCString(&ValueBuffer, "OK, this is, yet again, some new and different data... again!");

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = NewVersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(NewVersion, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_via_FORCE_write_mode_enabled(void)
{
    LOG_LOCATION;
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendCString(&ValueBuffer, "...blah blah!");

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_again_via_FORCE_with_garbage_version(void)
{
    LOG_LOCATION;
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendCString(&ValueBuffer, "Zippety do dah, zippedy eh...");

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_be_able_to_store_max_sized_entry(void)
{
    LOG_LOCATION;

    uint8_t keyBytes[] = {0x31,0x34,0x31,0x33,0x35,0x35,0x38,0x31,0x35,0x30,0x5F,0x30,0x30,0x30,0x30,0x5F,0x30,0x30};
    uint8_t versionBytes[] = {0x76,0x31,0x2E,0x30};
    uint8_t tagBytes[] = {0x73,0x6F,0x6D,0x65,0x5F,0x76,0x61,0x6C,0x75,0x65,0x5F,0x74,0x61,0x67,0x2E,0x2E,0x2E};

    ByteBuffer_Reset(&KeyBuffer);
    ByteBuffer_Append(&KeyBuffer, keyBytes, sizeof(keyBytes));
    ByteBuffer_Reset(&VersionBuffer);
    ByteBuffer_Append(&VersionBuffer, versionBytes, sizeof(versionBytes));
    ByteBuffer_Reset(&TagBuffer);
    ByteBuffer_Append(&TagBuffer, tagBytes, sizeof(tagBytes));
    ByteBuffer_Reset(&ValueBuffer);
    // ByteBuffer_AppendDummyData(&ValueBuffer, ValueBuffer.array.len);
    ByteBuffer_AppendDummyData(&ValueBuffer, 1024 * 128);

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .dbVersion = NewVersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
        .force = true,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Key, Entry.key.array);
    // TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_TRUE(Entry.force);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
    TEST_ASSERT_EQUAL(KINETIC_SYNCHRONIZATION_WRITETHROUGH, Entry.synchronization);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
