/*i
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
    SystemTestSetup(&Fixture, 1);
    KeyBuffer = ByteBuffer_CreateAndAppendCString(KeyData, sizeof(KeyData), "PUT test key");
    OtherKeyBuffer = ByteBuffer_CreateAndAppendCString(OtherKeyData, sizeof(OtherKeyData), "Some other PUT test key");
    TagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "SomeTagValue");
    VersionBuffer = ByteBuffer_CreateAndAppendCString(VersionData, sizeof(VersionData), "v1.0");
    NewVersionBuffer = ByteBuffer_CreateAndAppendCString(NewVersionData, sizeof(NewVersionData), "v2.0");
    OtherVersionBuffer = ByteBuffer_CreateAndAppendCString(OtherVersionData, sizeof(OtherVersionData), "v3.0");
    ValueBuffer = ByteBuffer_CreateAndAppendCString(ValueData, sizeof(ValueData), "lorem ipsum... blah blah blah... etc.");
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
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

    KineticStatus status = KineticClient_Put(&Fixture.session, &Entry, NULL);
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

    KineticStatus status = KineticClient_Put(&Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, Entry.dbVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);
    TEST_ASSERT_ByteBuffer_NULL(Entry.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(OtherKeyBuffer, Entry.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, Entry.tag);

    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, Entry.algorithm);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
