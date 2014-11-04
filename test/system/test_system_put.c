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
#include <time.h>

static SystemTestFixture Fixture;
static KineticEntry Entry;
static uint8_t KeyData[1024];
static ByteBuffer KeyBuffer;
static uint8_t OtherKeyData[1024];
static ByteBuffer OtherKeyBuffer;
static uint8_t TagData[1024];
static ByteBuffer TagBuffer;
static uint8_t VersionData[1024];
static ByteBuffer VersionBuffer;
static uint8_t NewVersionData[1024];
static ByteBuffer NewVersionBuffer;
static uint8_t OtherVersionData[1024];
static ByteBuffer OtherVersionBuffer;
static uint8_t ValueData[KINETIC_OBJ_SIZE];
static ByteBuffer ValueBuffer;

void setUp(void)
{
    SystemTestSetup(&Fixture);

    KeyBuffer = ByteBuffer_Create(KeyData, sizeof(KeyData), 0);
    ByteBuffer_AppendCString(&KeyBuffer, "PUT test key");
    OtherKeyBuffer = ByteBuffer_Create(OtherKeyData, sizeof(OtherKeyData), 0);;
    ByteBuffer_AppendCString(&OtherKeyBuffer, "Some other PUT test key");
    TagBuffer = ByteBuffer_Create(TagData, sizeof(TagData), 0);
    ByteBuffer_AppendCString(&TagBuffer, "SomeTagValue");
    VersionBuffer = ByteBuffer_Create(VersionData, sizeof(VersionData), 0);
    ByteBuffer_AppendCString(&VersionBuffer, "v1.0");
    NewVersionBuffer = ByteBuffer_Create(NewVersionData, sizeof(NewVersionData), 0);
    ByteBuffer_AppendCString(&NewVersionBuffer, "v2.0");
    OtherVersionBuffer = ByteBuffer_Create(OtherVersionData, sizeof(OtherVersionData), 0);
    ByteBuffer_AppendCString(&OtherVersionBuffer, "v3.0");
    ValueBuffer = ByteBuffer_Create(ValueData, sizeof(ValueData), 0);
    ByteBuffer_AppendCString(&ValueBuffer, "lorem ipsum... blah blah blah... etc.");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_Delete_old_object_if_exists(void)
{
    KineticStatus status;

    // Delete the first object, if it exists
    ByteBuffer_Reset(&TagBuffer);
    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .metadataOnly = true
    };
    status = KineticClient_Get(Fixture.handle, &Entry, NULL);
    if (status == KINETIC_STATUS_SUCCESS) {
        status = KineticClient_Delete(Fixture.handle, &Entry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    else {
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    }

    // Delete the second object, if it exists
    ByteBuffer_Reset(&TagBuffer);
    Entry = (KineticEntry) {
        .key = OtherKeyBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
    };
    status = KineticClient_Get(Fixture.handle, &Entry, NULL);
    if (status == KINETIC_STATUS_SUCCESS) {
        status = KineticClient_Delete(Fixture.handle, &Entry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    else {
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
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, Entry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);
    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);

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
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, Entry.dbVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(OtherKeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

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

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_ByteBuffer_EMPTY(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(NewVersionBuffer, Entry.dbVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

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
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(NewVersionBuffer, Entry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

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

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(NewVersionBuffer, Entry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

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

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, Entry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

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

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

void test_Put_should_be_able_to_store_max_sized_entry(void)
{
    LOG_LOCATION;

    uint8_t keyBytes[] = {0x31,0x34,0x31,0x33,0x35,0x35,0x38,0x31,0x35,0x30,0x5F,0x30,0x30,0x30,0x30,0x5F,0x30,0x30};

    ByteBuffer_Reset(&KeyBuffer);
    ByteBuffer_Append(&KeyBuffer, keyBytes, sizeof(keyBytes));
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendDummyData(&ValueBuffer, 1024 * 128);

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .value = ValueBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        .force = true,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
}

typedef struct _TestPutClientDataStruct {
    bool called;
    int callbackCount;
    int bytesWritten;
    KineticStatus status;
} TestPutClientDataStruct;

static TestPutClientDataStruct TestPutClientData;

void TestPutClosure(KineticCompletionData* kinetic_data, void* client_data)
{
    TEST_ASSERT_NOT_NULL(kinetic_data);
    KineticCompletionData* kdata = kinetic_data;

    TEST_ASSERT_EQUAL_PTR(&TestPutClientData, client_data);
    TestPutClientDataStruct* cdata = client_data;
    TEST_ASSERT_EQUAL_PTR(&TestPutClientData, cdata);

    cdata->called = true;
    cdata->bytesWritten += 100;
    cdata->status = kdata->status;
    cdata->callbackCount++;
}

void test_Put_should_use_asynchronous_mode_if_closure_specified(void)
{
    LOG_LOCATION;

    uint8_t keyBytes[] = {0x31,0x34,0x31,0x33,0x35,0x35,0x38,0x31,0x35,0x30,0x5F,0x30,0x30,0x30,0x30,0x5F,0x30,0x30};

    ByteBuffer_Reset(&KeyBuffer);
    ByteBuffer_Append(&KeyBuffer, keyBytes, sizeof(keyBytes));
    ByteBuffer_Reset(&ValueBuffer);
    ByteBuffer_AppendDummyData(&ValueBuffer, 1024 * 128);

    Entry = (KineticEntry) {
        .key = KeyBuffer,
        .value = ValueBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        .force = true,
    };

    memset(&TestPutClientData, 0, sizeof(TestPutClientData));
    KineticCompletionClosure closure = {
        .callback = &TestPutClosure,
        .clientData = &TestPutClientData,
    };

    KineticStatus status = KineticClient_Put(Fixture.handle, &Entry, &closure);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    const long maxWaitMillisecs = 500;
    long millisecsWaiting = 0;
    struct timespec sleepDuration = {.tv_nsec = 1000000};
    while(!TestPutClientData.called) {
        TEST_ASSERT_TRUE_MESSAGE(millisecsWaiting < maxWaitMillisecs, "Timed out waiting to receive PUT async callback!");
        nanosleep(&sleepDuration, NULL);
        millisecsWaiting += (sleepDuration.tv_nsec / 1000000);
    }

    LOGF0("PUT Response Time = %d ms", millisecsWaiting);

    TEST_ASSERT_EQUAL(1, TestPutClientData.callbackCount);
    TEST_ASSERT_EQUAL(100, TestPutClientData.bytesWritten);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, TestPutClientData.status);

    TEST_ASSERT_EQUAL_ByteBuffer(KeyBuffer, Entry.key);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
