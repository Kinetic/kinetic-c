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
    SystemTestSetup(&Fixture, 1);
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

static bool add_keys(int count)
{
    static const ssize_t sz = 10;
    char key_buf[sz];
    char tag_buf[sz];
    char value_buf[sz];

    for (int i = 0; i < count; i++) {

        KineticEntry entry = {
            .key = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key_%d", i),
            .tag = ByteBuffer_CreateAndAppendFormattedCString(tag_buf, sz, "tag_%d", i),
            .value = ByteBuffer_CreateAndAppendFormattedCString(value_buf, sz, "val_%d", i),
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .force = true,
        };

        KineticStatus status = KineticClient_Put(&Fixture.session, &entry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    return true;
}

typedef enum { CMD_NEXT, CMD_PREVIOUS } GET_CMD;

static void compare_against_offset_key(GET_CMD cmd, bool metadataOnly)
{
    static const ssize_t sz = 10;
    char key_buf[sz];
    char tag_buf[sz];
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
        ByteBuffer keyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key_%d", i + offset);
        ByteBuffer tagBuffer = ByteBuffer_CreateAndAppendFormattedCString(tag_buf, sz, "tag_%d", i + offset);
        ByteBuffer valueBuffer = ByteBuffer_Create(value_buf, sz, 0);

        printf("KEY '%s'\n", key_buf);

        KineticEntry entry = {
            .key = keyBuffer,
            .tag = tagBuffer,
            .value = valueBuffer,
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

        ByteBuffer expectedKeyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_exp_buf, sz, "key_%d", i);
        ByteBuffer expectedValueBuffer = ByteBuffer_CreateAndAppendFormattedCString(value_exp_buf, sz, "val_%d", i);

        TEST_ASSERT_EQUAL_ByteBuffer(expectedKeyBuffer, entry.key);
        if (!metadataOnly) {
            TEST_ASSERT_EQUAL_ByteBuffer(expectedValueBuffer, entry.value);
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
