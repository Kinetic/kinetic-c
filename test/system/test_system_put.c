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

static SystemTestFixture Fixture;
static ByteArray ValueKey;
static ByteArray OtherValueKey;
static ByteArray Tag;
static ByteArray TestValue;
static ByteArray Version;
static ByteArray NewVersion;
static ByteArray OtherNewVersion;
static ByteArray GarbageVersion;
static KineticEntry Entry;

void setUp(void)
{
    SystemTestSetup(&Fixture);
    ValueKey = ByteArray_CreateWithCString("my_key_3.1415927");
    OtherValueKey = ByteArray_CreateWithCString("my_key_3.1415927_0");
    Tag = ByteArray_CreateWithCString("SomeTagValue");
    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah... etc...");
    Version = ByteArray_CreateWithCString("v1.0");
    NewVersion = ByteArray_CreateWithCString("v2.0");
    OtherNewVersion = ByteArray_CreateWithCString("v3.0");
    GarbageVersion = ByteArray_CreateWithCString("some...garbage..&$*#^@");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_Put_should_create_new_object_on_device(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .newVersion = ByteBuffer_CreateWithArray(Version),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_ByteBuffer_EMPTY(Entry.dbVersion);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_create_another_new_object_on_device(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(OtherValueKey),
        .newVersion = ByteBuffer_CreateWithArray(Version),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_ByteBuffer_EMPTY(Entry.dbVersion);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(OtherValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .dbVersion = ByteBuffer_CreateWithArray(Version),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(Version, Entry.dbVersion.array);
    TEST_ASSERT_ByteBuffer_EMPTY(Entry.dbVersion);
    TEST_ASSERT_ByteArray_NONE(Entry.newVersion.array);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);

    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .newVersion = ByteBuffer_CreateWithArray(OtherNewVersion),
        .dbVersion = ByteBuffer_CreateWithArray(NewVersion),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_and_with_FLUSH_sync_mode_enabled(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .dbVersion = ByteBuffer_CreateWithArray(OtherNewVersion),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .synchronization = KINETIC_SYNCHRONIZATION_FLUSH,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_via_FORCE_write_mode_enabled(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_again_via_FORCE_with_garbage_version(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_via_FORCE_with_invalid_dbVersion(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .dbVersion = ByteBuffer_CreateWithArray(NewVersion),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_update_object_data_on_device_via_FORCE_write_mode_enabled_and_unused_newVersion(void)
{
    LOG(""); LOG_LOCATION;
    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateWithArray(ValueKey),
        .newVersion = ByteBuffer_CreateWithArray(NewVersion),
        .tag = ByteBuffer_CreateWithArray(Tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(TestValue),
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(ValueKey, Entry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(Tag, Entry.tag.array);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, Entry.value.array);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
