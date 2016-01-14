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

static bool add_keys(int count)
{
    static const ssize_t sz = 10;
    char key_buf[sz];
    char tag_buf[sz];
    char value_buf[sz];

    for (int i = 0; i < count; i++) {

        KineticEntry entry = {
            .key = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "mykey_%02d", i),
            .tag = ByteBuffer_CreateAndAppendFormattedCString(tag_buf, sz, "mytag_%02d", i),
            .value = ByteBuffer_CreateAndAppendFormattedCString(value_buf, sz, "val_%02d", i),
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .force = true,
        };

        KineticStatus status = KineticClient_Put(Fixture.session, &entry, NULL);
        if (KINETIC_STATUS_SUCCESS != status) { return false; }
    }
    return true;
}

void setUp(void)
{
    SystemTestSetup(1, true);
    assert(add_keys(3));
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_GetKeyRange_should_retrieve_a_range_of_keys_from_device(void)
{
    const size_t numKeys = 3;
    const size_t keyLen = 64;
    uint8_t startKeyData[keyLen], endKeyData[keyLen];
    KineticKeyRange range = {
        .startKey = ByteBuffer_CreateAndAppendCString(startKeyData, sizeof(startKeyData), "mykey_"),
        .endKey = ByteBuffer_CreateAndAppendCString(endKeyData, sizeof(endKeyData), "mykey_01"),
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = 2,
    };
    uint8_t keysData[numKeys][keyLen];
    for (size_t i = 0; i < numKeys; i++) {
        memset(&keysData[i], 0, keyLen);
    }
    ByteBuffer keyBuff[] = {
        ByteBuffer_Create(&keysData[0], keyLen, 0),
        ByteBuffer_Create(&keysData[1], keyLen, 0),
        ByteBuffer_Create(&keysData[2], keyLen, 0),
    };
    ByteBufferArray keys = {.buffers = &keyBuff[0], .count = numKeys};

    KineticStatus status = KineticClient_GetKeyRange(Fixture.session, &range, &keys, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(2, keys.used);
    TEST_ASSERT_EQUAL_STRING("mykey_00", keyBuff[0].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_00"), keyBuff[0].bytesUsed);
    TEST_ASSERT_EQUAL_STRING("mykey_01", keyBuff[1].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_01"), keyBuff[1].bytesUsed);
    TEST_ASSERT_ByteBuffer_EMPTY(keyBuff[2]);
}

void test_GetKeyRange_should_retrieve_a_range_of_keys_from_device_in_reverse_order(void)
{
    const size_t numKeys = 3;
    const size_t keyLen = 64;
    uint8_t startKeyData[keyLen], endKeyData[keyLen];
    KineticKeyRange range = {
        .startKey = ByteBuffer_CreateAndAppendCString(startKeyData, sizeof(startKeyData), "mykey_00"),
        .endKey = ByteBuffer_CreateAndAppendCString(endKeyData, sizeof(endKeyData), "mykey_02"),
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = 3,
        .reverse = true,
    };
    uint8_t keysData[numKeys][keyLen];
    for (size_t i = 0; i < numKeys; i++) {
        memset(&keysData[i], 0, keyLen);
    }
    ByteBuffer keyBuff[] = {
        ByteBuffer_Create(&keysData[0], keyLen, 0),
        ByteBuffer_Create(&keysData[1], keyLen, 0),
        ByteBuffer_Create(&keysData[2], keyLen, 0),
    };
    ByteBufferArray keys = {.buffers = &keyBuff[0], .count = numKeys};

    KineticStatus status = KineticClient_GetKeyRange(Fixture.session, &range, &keys, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(3, keys.used);
    TEST_ASSERT_EQUAL_STRING("mykey_02", keyBuff[0].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_02"), keyBuff[0].bytesUsed);
    TEST_ASSERT_EQUAL_STRING("mykey_01", keyBuff[1].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_01"), keyBuff[1].bytesUsed);
    TEST_ASSERT_EQUAL_STRING("mykey_00", keyBuff[2].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_00"), keyBuff[2].bytesUsed);
}

void test_GetKeyRange_should_retrieve_a_range_of_keys_from_device_with_start_and_end_keys_excluded(void)
{
    const size_t numKeys = 3;
    const size_t keyLen = 64;
    uint8_t startKeyData[keyLen], endKeyData[keyLen];
    KineticKeyRange range = {
        .startKey = ByteBuffer_CreateAndAppendCString(startKeyData, sizeof(startKeyData), "mykey_00"),
        .endKey = ByteBuffer_CreateAndAppendCString(endKeyData, sizeof(endKeyData), "mykey_02"),
        .startKeyInclusive = false,
        .endKeyInclusive = false,
        .maxReturned = 2,
    };
    uint8_t keysData[numKeys][keyLen];
    for (size_t i = 0; i < numKeys; i++) {
        memset(&keysData[i], 0, keyLen);
    }
    ByteBuffer keyBuff[] = {
        ByteBuffer_Create(&keysData[0], keyLen, 0),
        ByteBuffer_Create(&keysData[1], keyLen, 0),
        ByteBuffer_Create(&keysData[2], keyLen, 0),
    };
    ByteBufferArray keys = {.buffers = &keyBuff[0], .count = numKeys};

    KineticStatus status = KineticClient_GetKeyRange(Fixture.session, &range, &keys, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(1, keys.used);
    TEST_ASSERT_EQUAL_STRING("mykey_01", keyBuff[0].array.data);
    TEST_ASSERT_EQUAL(strlen("mykey_01"), keyBuff[0].bytesUsed);
    TEST_ASSERT_ByteBuffer_EMPTY(keyBuff[1]);
    TEST_ASSERT_ByteBuffer_EMPTY(keyBuff[2]);
}
