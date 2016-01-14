/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "system_test_fixture.h"
#include "kinetic_client.h"

void setUp(void)
{
    SystemTestSetup(1, true);
}

void tearDown(void)
{
    SystemTestShutDown();
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

        KineticStatus status = KineticClient_Put(Fixture.session, &entry, NULL);
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
            status = KineticClient_GetNext(Fixture.session, &entry, NULL);
            break;
        case CMD_PREVIOUS:
            status = KineticClient_GetPrevious(Fixture.session, &entry, NULL);
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
