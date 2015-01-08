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

static SystemTestFixture Fixture;

void setUp(void)
{
    LOG_LOCATION;
    SystemTestSetup(&Fixture, 1);
}

void tearDown(void)
{
    LOG_LOCATION;
    SystemTestTearDown(&Fixture);
}

static bool add_keys(int count)
{
    static const ssize_t sz = 10;
    char key_buf[sz];
    char value_buf[sz];

    for (int i = 0; i < count; i++) {
        #if 0
        if (sz < snprintf(key_buf, sz, "key_%d", i)) { return false; }
        if (sz < snprintf(value_buf, sz, "val_%d", i)) { return false; }
        ByteBuffer KeyBuffer = ByteBuffer_CreateWithArray(ByteArray_CreateWithCString(key_buf));
        ByteBuffer ValueBuffer = ByteBuffer_CreateWithArray(ByteArray_CreateWithCString(value_buf));
        #endif

        ByteBuffer KeyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key_%d", i);
        ByteBuffer ValueBuffer = ByteBuffer_CreateAndAppendFormattedCString(value_buf, sz, "val_%d", i);

        KineticEntry entry = {
            .key = KeyBuffer,
            .value = ValueBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .force = true,
        };

        KineticStatus status = KineticClient_Put(&Fixture.session, &entry, NULL);
        if (KINETIC_STATUS_SUCCESS != status) { return false; }
    }
    return true;
}

typedef enum { CMD_NEXT, CMD_PREVIOUS } GET_CMD;

static void compare_against_offset_key(GET_CMD cmd, bool metadataOnly)
{
    LOG_LOCATION;
    static const ssize_t sz = 10;
    char key_buf[sz];
    char value_buf[sz];
    char key_exp_buf[sz];
    char value_exp_buf[sz];

    const int count = 3;

    TEST_ASSERT_TRUE(add_keys(count));    /* add keys [key_X -> test_X] */

    int low = 0;
    int high = count;
    int8_t offset = 0;

    switch (cmd) {
    case CMD_NEXT:
        low = 1;
        offset = -1;
        break;
    case CMD_PREVIOUS:
        high = count - 1;
        offset = +1;
        break;
    default:
        TEST_ASSERT(false);
    }

    for (int i = low; i < high; i++) {
        ByteBuffer KeyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key_%d", i + offset);

        ByteBuffer ValueBuffer = ByteBuffer_Create(value_buf, sz, 0);

        printf("KEY '%s'\n", key_buf);

        KineticEntry entry = {
            .key = KeyBuffer,
            .value = ValueBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .metadataOnly = metadataOnly,
        };
        KineticStatus status = KINETIC_STATUS_INVALID;

        switch (cmd) {
        case CMD_NEXT:
            status = KineticClient_GetNext(&Fixture.session, &entry, NULL);
            break;
        case CMD_PREVIOUS:
            status = KineticClient_GetPrevious(&Fixture.session, &entry, NULL);
            break;
        default:
            TEST_ASSERT(false);
        }

        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

#if 0
        TEST_ASSERT_FALSE(sz < snprintf(key_exp_buf, sz, "key_%d", i));
        TEST_ASSERT_FALSE(sz < snprintf(value_exp_buf, sz, "val_%d", i));
        ByteBuffer ExpectedKeyBuffer = ByteBuffer_CreateWithArray(ByteArray_CreateWithCString(key_exp_buf));
        ByteBuffer ExpectedValueBuffer = ByteBuffer_CreateWithArray(ByteArray_CreateWithCString(value_exp_buf));
#endif
        ByteBuffer ExpectedKeyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_exp_buf, sz, "key_%d", i);
        ByteBuffer ExpectedValueBuffer = ByteBuffer_CreateAndAppendFormattedCString(value_exp_buf, sz, "val_%d", i);

        TEST_ASSERT_EQUAL_ByteBuffer(ExpectedKeyBuffer, entry.key);
        if (!metadataOnly) {
            TEST_ASSERT_EQUAL_ByteBuffer(ExpectedValueBuffer, entry.value);
        }
    }
}

void test_GetNext_should_retrieve_object_and_metadata_from_device(void)
{
    compare_against_offset_key(CMD_NEXT, false);
}

void test_GetNext_should_retrieve_only_metadata_with_metadataOnly(void)
{
    compare_against_offset_key(CMD_NEXT, true);
}

void test_GetPrevious_should_retrieve_object_and_metadata_from_device(void)
{
    compare_against_offset_key(CMD_PREVIOUS, false);
}

void test_GetPrevious_should_retrieve_only_metadata_with_metadataOnly(void)
{
    compare_against_offset_key(CMD_PREVIOUS, true);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
