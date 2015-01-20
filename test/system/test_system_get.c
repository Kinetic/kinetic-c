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
#include "system_test_fixture.h"
#include "kinetic_client.h"

static uint8_t KeyData[1024];
static ByteBuffer KeyBuffer;
static uint8_t ExpectedKeyData[1024];
static ByteBuffer ExpectedKeyBuffer;
static uint8_t TagData[1024];
static ByteBuffer TagBuffer;
static uint8_t ExpectedTagData[1024];
static ByteBuffer ExpectedTagBuffer;
static uint8_t VersionData[1024];
static ByteBuffer VersionBuffer;
static uint8_t ExpectedVersionData[1024];
static ByteBuffer ExpectedVersionBuffer;
static ByteArray TestValue;
static uint8_t ValueData[KINETIC_OBJ_SIZE];
static ByteBuffer ValueBuffer;
static const char strKey[] = "GET system test blob";

static bool initialized = false;

void setUp(void)
{ LOG_LOCATION;
    SystemTestSetup(2);

    if (!initialized) {
        KeyBuffer = ByteBuffer_CreateAndAppendCString(KeyData, sizeof(KeyData), strKey);
        ExpectedKeyBuffer = ByteBuffer_CreateAndAppendCString(ExpectedKeyData, sizeof(ExpectedKeyData), strKey);
        TagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "SomeTagValue");
        ExpectedTagBuffer = ByteBuffer_CreateAndAppendCString(ExpectedTagData, sizeof(ExpectedTagData), "SomeTagValue");
        VersionBuffer = ByteBuffer_CreateAndAppendCString(VersionData, sizeof(VersionData), "v1.0");
        ExpectedVersionBuffer = ByteBuffer_CreateAndAppendCString(ExpectedVersionData, sizeof(ExpectedVersionData), "v1.0");
        TestValue = ByteArray_CreateWithCString("lorem ipsum... blah blah blah... etc.");
        ValueBuffer = ByteBuffer_CreateAndAppendArray(ValueData, sizeof(ValueData), TestValue);

        // Setup to write some test data
        KineticEntry putEntry = {
            .key = KeyBuffer,
            .tag = TagBuffer,
            .newVersion = VersionBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = ValueBuffer,
            .force = true,
            .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };

        KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        TEST_ASSERT_EQUAL_ByteBuffer(ExpectedKeyBuffer, putEntry.key);
        TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, putEntry.tag);
        TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, putEntry.dbVersion);
        TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, putEntry.algorithm);
        TEST_ASSERT_ByteBuffer_NULL(putEntry.newVersion);

        initialized = true;
    }
}

void tearDown(void)
{ LOG_LOCATION;
    SystemTestShutDown();
}

void test_Get_should_retrieve_object_and_metadata_from_device(void)
{ LOG_LOCATION;

    KineticEntry getEntry = {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedKeyBuffer, getEntry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
    uint8_t expectedValueData[128];
    ByteBuffer expectedValue = ByteBuffer_CreateAndAppendArray(expectedValueData, sizeof(expectedValueData), TestValue);
    TEST_ASSERT_EQUAL_ByteBuffer(expectedValue, getEntry.value);
}

void test_Get_should_retrieve_object_and_metadata_from_device_again(void)
{ LOG_LOCATION;

    KineticEntry getEntry = {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
    };

    KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedKeyBuffer, getEntry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
    uint8_t expectedValueData[128];
    ByteBuffer expectedValue = ByteBuffer_CreateAndAppendArray(expectedValueData, sizeof(expectedValueData), TestValue);
    TEST_ASSERT_EQUAL_ByteBuffer(expectedValue, getEntry.value);
}

void test_Get_should_be_able_to_retrieve_just_metadata_from_device(void)
{ LOG_LOCATION;

    KineticEntry getEntry = {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .metadataOnly = true,
    };

    KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedKeyBuffer, getEntry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
}
