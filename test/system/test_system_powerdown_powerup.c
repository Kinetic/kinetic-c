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
#include "kinetic_admin_client.h"

static uint8_t KeyData[1024];
static ByteBuffer KeyBuffer;
static uint8_t ExpectedKeyData[1024];
static ByteBuffer ExpectedKeyBuffer;
static uint8_t TagData[1024];
static ByteBuffer TagBuffer;
static uint8_t ExpectedTagData[1024];
static ByteBuffer ExpectedTagBuffer;
static ByteArray TestValue;
static uint8_t ValueData[KINETIC_OBJ_SIZE];
static ByteBuffer ValueBuffer;
static const char strKey[] = "GET system test blob";

void setUp(void)
{
    SystemTestSetup(1);

    KeyBuffer = ByteBuffer_CreateAndAppendCString(KeyData, sizeof(KeyData), strKey);
    ExpectedKeyBuffer = ByteBuffer_CreateAndAppendCString(ExpectedKeyData, sizeof(ExpectedKeyData), strKey);
    TagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "SomeTagValue");
    ExpectedTagBuffer = ByteBuffer_CreateAndAppendCString(ExpectedTagData, sizeof(ExpectedTagData), "SomeTagValue");
    TestValue = ByteArray_CreateWithCString("lorem ipsum... blah blah blah... etc.");
    ValueBuffer = ByteBuffer_CreateAndAppendArray(ValueData, sizeof(ValueData), TestValue);

    // Setup to write some test data
    KineticEntry putEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_FLUSH,
    };

    KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);

    // Validate the object exists initially
    KineticEntry getEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Get(Fixture.session, &getEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.key.array, getEntry.key.array);
    TEST_ASSERT_EQUAL_ByteArray(putEntry.tag.array, getEntry.tag.array);
    TEST_ASSERT_EQUAL(putEntry.algorithm, getEntry.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(putEntry.value, getEntry.value);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_KineticAdmin_should_powerdown_and_powerup_a_device(void)
{
    KineticStatus status;

    status = KineticAdminClient_PowerDown(Fixture.adminSession);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

	KineticEntry getEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
        .force = true,
    };

    /* Currently, the device should be powered down*/
    if (SystemTestIsUnderSimulator()) {
        // Validate the object cannot being accessed while powered down
        status = KineticClient_Get(Fixture.session, &getEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_POWER_DOWN, status);
    }
    
    status = KineticAdminClient_PowerUp(Fixture.adminSession);
	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
	
    /* Currently, the device should be powered up*/
    if (SystemTestIsUnderSimulator()) {
        // Validate the object can being accessed again while powered up 
        status = KineticClient_Get(Fixture.session, &getEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
}
