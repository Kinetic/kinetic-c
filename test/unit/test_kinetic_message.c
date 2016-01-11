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

#include "unity.h"
#include "unity_helper.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic.pb-c.h"
#include "kinetic_message.h"
#include "kinetic_logger.h"

uint8_t KeyData[1024];
ByteArray Key;
ByteBuffer KeyBuffer;
uint8_t NewVersionData[1024];
ByteArray NewVersion;
ByteBuffer NewVersionBuffer;
uint8_t VersionData[1024];
ByteArray Version;
ByteBuffer VersionBuffer;
uint8_t TagData[1024];
ByteArray Tag;
ByteBuffer TagBuffer;

void setUp(void)
{
    memset(KeyData, 0, sizeof(KeyData));
    Key = ByteArray_Create(KeyData, sizeof(Key));
    KeyBuffer = ByteBuffer_CreateWithArray(Key);
    ByteBuffer_AppendCString(&KeyBuffer, "my_key_3.1415927");
    memset(NewVersionData, 0, sizeof(KeyData));
    NewVersion = ByteArray_Create(NewVersionData, sizeof(NewVersion));
    NewVersionBuffer = ByteBuffer_CreateWithArray(NewVersion);
    ByteBuffer_AppendCString(&NewVersionBuffer, "v2.0");
    memset(VersionData, 0, sizeof(KeyData));
    Version = ByteArray_Create(VersionData, sizeof(Version));
    VersionBuffer = ByteBuffer_CreateWithArray(Version);
    ByteBuffer_AppendCString(&VersionBuffer, "v1.0");
    memset(TagData, 0, sizeof(KeyData));
    Tag = ByteArray_Create(TagData, sizeof(Tag));
    TagBuffer = ByteBuffer_CreateWithArray(Tag);
    ByteBuffer_AppendCString(&TagBuffer, "SomeTagValue");
}

void tearDown(void)
{
}


void test_KineticMessage_Init_should_initialize_the_message_and_required_protobuf_fields(void)
{
    KineticMessage protoMsg;

    KineticMessage_Init(&protoMsg);
}

void test_KineticMessage_ConfigureKeyValue_should_configure_Body_KeyValue_and_add_to_message(void)
{
    KineticMessage message;

    KineticEntry entry = {
        .key = KeyBuffer,
        .newVersion = NewVersionBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .metadataOnly = true,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    memset(&message, 0, sizeof(KineticMessage));
    KineticMessage_Init(&message);

    KineticMessage_ConfigureKeyValue(&message, &entry);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.keyValue, message.command.body->keyvalue);

    // Validate keyValue fields
    TEST_ASSERT_TRUE(message.keyValue.has_newversion);
    TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(message.keyValue.newversion, entry.newVersion);
    TEST_ASSERT_TRUE(message.keyValue.has_key);
    TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(message.keyValue.key, entry.key);
    TEST_ASSERT_TRUE(message.keyValue.has_dbversion);
    TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(message.keyValue.dbversion, entry.dbVersion);
    TEST_ASSERT_TRUE(message.keyValue.has_tag);
    TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(message.keyValue.tag, entry.tag);
    TEST_ASSERT_TRUE(message.keyValue.has_algorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA1, message.keyValue.algorithm);
    TEST_ASSERT_TRUE(message.keyValue.has_metadataonly);
    TEST_ASSERT_TRUE(message.keyValue.metadataonly);
    TEST_ASSERT_TRUE(message.keyValue.has_force);
    TEST_ASSERT_TRUE(message.keyValue.force);
    TEST_ASSERT_TRUE(message.keyValue.has_synchronization);
    TEST_ASSERT_EQUAL(KINETIC_SYNCHRONIZATION_WRITETHROUGH, message.keyValue.synchronization);


    entry = (KineticEntry) {
        .metadataOnly = false
    };
    memset(&message, 0, sizeof(KineticMessage));
    KineticMessage_Init(&message);

    KineticMessage_ConfigureKeyValue(&message, &entry);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.keyValue, message.command.body->keyvalue);

    // Validate keyValue fields
    TEST_ASSERT_FALSE(message.keyValue.has_newversion);
    TEST_ASSERT_FALSE(message.keyValue.has_key);
    TEST_ASSERT_FALSE(message.keyValue.has_dbversion);
    TEST_ASSERT_FALSE(message.keyValue.has_tag);
    TEST_ASSERT_FALSE(message.keyValue.has_algorithm);
    TEST_ASSERT_FALSE(message.keyValue.has_metadataonly);
    TEST_ASSERT_FALSE(message.keyValue.has_force);
    TEST_ASSERT_FALSE(message.keyValue.has_synchronization);
}

void test_KineticMessage_ConfigureKeyRange_should_add_and_configure_a_KineticProto_KeyRange_to_the_message(void)
{
    KineticMessage message;

    const int numKeysInRange = 4;
    uint8_t startKeyData[32];
    uint8_t endKeyData[32];
    ByteBuffer startKey, endKey;

    startKey = ByteBuffer_Create(startKeyData, sizeof(startKeyData), 0);
    ByteBuffer_AppendCString(&startKey, "key_range_00_00");
    endKey = ByteBuffer_Create(endKeyData, sizeof(endKeyData), 0);
    ByteBuffer_AppendCString(&endKey, "key_range_00_03");

    KineticKeyRange range = {
        .startKey = startKey,
        .endKey = endKey,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = numKeysInRange,
        .reverse = true,
    };

    memset(&message, 0, sizeof(KineticMessage));
    KineticMessage_Init(&message);

    KineticMessage_ConfigureKeyRange(&message, &range);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.keyRange, message.command.body->range);

    // Validate range fields
    TEST_ASSERT_TRUE(message.command.body->range->has_startkey);
    TEST_ASSERT_EQUAL_PTR(startKey.array.data, message.command.body->range->startkey.data);
    TEST_ASSERT_EQUAL(startKey.bytesUsed, message.command.body->range->startkey.len);
    TEST_ASSERT_TRUE(message.command.body->range->has_endkey);
    TEST_ASSERT_EQUAL_PTR(endKey.array.data, message.command.body->range->endkey.data);
    TEST_ASSERT_EQUAL(endKey.bytesUsed, message.command.body->range->endkey.len);
    TEST_ASSERT_TRUE(message.command.body->range->has_startkeyinclusive);
    TEST_ASSERT_TRUE(message.command.body->range->startkeyinclusive);
    TEST_ASSERT_TRUE(message.command.body->range->has_endkeyinclusive);
    TEST_ASSERT_TRUE(message.command.body->range->endkeyinclusive);
    TEST_ASSERT_TRUE(message.command.body->range->has_maxreturned);
    TEST_ASSERT_EQUAL(numKeysInRange, message.command.body->range->maxreturned);
    TEST_ASSERT_TRUE(message.command.body->range->has_reverse);
    TEST_ASSERT_TRUE(message.command.body->range->reverse);
    TEST_ASSERT_EQUAL(0, message.command.body->range->n_keys);
    TEST_ASSERT_NULL(message.command.body->range->keys);

    range = (KineticKeyRange) {
        .startKey = startKey,
        .endKey = endKey,
        .startKeyInclusive = false,
        .endKeyInclusive = false,
        .maxReturned = 1,
        .reverse = false,
    };

    memset(&message, 0, sizeof(KineticMessage));
    KineticMessage_Init(&message);

    KineticMessage_ConfigureKeyRange(&message, &range);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.keyRange, message.command.body->range);

    // Validate range fields
    TEST_ASSERT_TRUE(message.command.body->range->has_startkey);
    TEST_ASSERT_EQUAL_PTR(startKey.array.data, message.command.body->range->startkey.data);
    TEST_ASSERT_EQUAL(startKey.bytesUsed, message.command.body->range->startkey.len);
    TEST_ASSERT_TRUE(message.command.body->range->has_endkey);
    TEST_ASSERT_EQUAL_PTR(endKey.array.data, message.command.body->range->endkey.data);
    TEST_ASSERT_EQUAL(endKey.bytesUsed, message.command.body->range->endkey.len);
    TEST_ASSERT_FALSE(message.command.body->range->has_startkeyinclusive);
    TEST_ASSERT_FALSE(message.command.body->range->has_endkeyinclusive);
    TEST_ASSERT_TRUE(message.command.body->range->has_maxreturned);
    TEST_ASSERT_EQUAL(1, message.command.body->range->maxreturned);
    TEST_ASSERT_FALSE(message.command.body->range->has_reverse);
    TEST_ASSERT_EQUAL(0, message.command.body->range->n_keys);
    TEST_ASSERT_NULL(message.command.body->range->keys);
}
