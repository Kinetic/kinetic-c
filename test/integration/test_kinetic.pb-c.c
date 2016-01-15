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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protobuf-c/protobuf-c.h"
#include "kinetic_types.h"
#include "kinetic.pb-c.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticProto_should_pack_and_unpack_protocol_buffers(void)
{
    size_t expectedLen;
    Com__Seagate__Kinetic__Proto__Message msg;
    Com__Seagate__Kinetic__Proto__Command cmd;
    Com__Seagate__Kinetic__Proto__Command__Header header;
    uint8_t buffer[256];
    ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT(buffer);
    uint8_t* packedMessage;
    uint8_t* packedCommand;

    com__seagate__kinetic__proto__message__init(&msg);
    com__seagate__kinetic__proto__command__init(&cmd);
    com__seagate__kinetic__proto__command__header__init(&header);

    cmd.header = &header;

    // msg.commandBytes = &cmd;

    expectedLen = com__seagate__kinetic__proto__command__get_packed_size(&cmd);
    packedCommand = malloc(com__seagate__kinetic__proto__command__get_packed_size(&cmd));
    TEST_ASSERT_NOT_NULL_MESSAGE(packedCommand,
                                 "Failed dynamically allocating command buffer");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__command__pack(&cmd, packedCommand),
                              "Packed size command invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__command__get_packed_size(&cmd),
                              "Inspected packed command size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__command__pack_to_buffer(&cmd, &bs.base),
                              "Packed BufferSimple command size invalid");


    expectedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    packedMessage = malloc(com__seagate__kinetic__proto__message__get_packed_size(&msg));
    TEST_ASSERT_NOT_NULL_MESSAGE(packedMessage,
                                 "Failed dynamically allocating buffer");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__message__pack(&msg, packedMessage),
                              "Packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__message__get_packed_size(&msg),
                              "Inspected packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen,
                              com__seagate__kinetic__proto__message__pack_to_buffer(&msg, &bs.base),
                              "Packed BufferSimple size invalid");

    // Validate the both packed buffers are equal
    // TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bs.data, packedMessage, expectedLen,
    //                                      "Packed proto buffers do not match");
    PROTOBUF_C_BUFFER_SIMPLE_CLEAR(&bs);

    // Free dynamically allocated memory
    free(packedMessage);
    free(packedCommand);
}

void test_KinetiProto_double_packing_should_work(void)
{
    size_t expectedPackedLen, actualPackedLen;
    uint8_t packBuf[128];

    Com__Seagate__Kinetic__Proto__Message msg;
    com__seagate__kinetic__proto__message__init(&msg);
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(expectedPackedLen == 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    msg.has_authtype = true;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    msg.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth;
    com__seagate__kinetic__proto__message__hmacauth__init(&hmacAuth);
    msg.hmacauth = &hmacAuth;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    hmacAuth.has_identity = true;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    hmacAuth.identity = 17;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);

    uint8_t hmacData[] = {1,2,3,4,5};
    hmacAuth.hmac = (ProtobufCBinaryData) {.data = hmacData, .len = sizeof(hmacData)};
    hmacAuth.has_hmac = true;
    expectedPackedLen = com__seagate__kinetic__proto__message__get_packed_size(&msg);
    TEST_ASSERT_TRUE(com__seagate__kinetic__proto__message__get_packed_size(&msg) > 0);
    actualPackedLen = com__seagate__kinetic__proto__message__pack(&msg, packBuf);
    TEST_ASSERT_EQUAL(expectedPackedLen, actualPackedLen);
}

#if 0
void test_KineticProto_should_pass_data_accurately_through_raw_buffers(void)
{
    uint8_t* packed;
    KineticProto* unpacked;
    KineticProto proto;
    Com__Seagate__Kinetic__Proto__Command cmd;
    Com__Seagate__Kinetic__Proto__Command__Header header;

    KineticProto__init(&proto);
    com__seagate__kinetic__proto__command__init(&cmd);
    proto.command = &cmd;
    com__seagate__kinetic__proto__command__header__init(&header);
    header.clusterversion = 12345678;
    header.has_clusterVersion = true;
    header.identity = -12345678;
    header.has_identity = true;
    cmd.header = &header;

    // Pack the buffer
    packed = malloc(com__seagate__kinetic__proto__message__get_packed_size(&proto));
    KineticProto__pack(&proto, packed);

    // Unpack and verify the raw buffer results
    unpacked = KineticProto__unpack(NULL,
                                    com__seagate__kinetic__proto__message__get_packed_size(&proto), packed);
    TEST_ASSERT_NOT_NULL_MESSAGE(unpacked,
                                 "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_identity,
                             "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul,
                                    unpacked->command->header->identity,
                                    "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_clusterVersion,
                             "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul,
                                    unpacked->command->header->clusterVersion,
                                    "Command header cluster version invalid");

    // Free dynamically allocated memory
    KineticProto__free_unpacked(unpacked, NULL);
    free(packed);
}

void test_KineticProto_should_pass_data_accurately_through_BufferSimple(void)
{
    KineticProto proto;
    KineticProto* unpacked;
    Com__Seagate__Kinetic__Proto__Command cmd;
    Com__Seagate__Kinetic__Proto__Command__Header header;
    uint8_t buffer[1024];
    ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT(buffer);

    KineticProto__init(&proto);
    com__seagate__kinetic__proto__command__init(&cmd);
    proto.command = &cmd;
    com__seagate__kinetic__proto__command__header__init(&header);
    header.clusterversion = 12345678;
    header.has_clusterVersion = true;
    header.identity = -12345678;
    header.has_identity = true;
    cmd.header = &header;

    // Pack the buffer
    KineticProto__pack_to_buffer(&proto, &bs.base);

    // Unpack and verify the simple buffer results
    unpacked = KineticProto__unpack(NULL,
                                    com__seagate__kinetic__proto__message__get_packed_size(&proto), bs.data);
    TEST_ASSERT_NOT_NULL_MESSAGE(unpacked, "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_identity,
                             "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul,
                                    unpacked->command->header->identity,
                                    "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_clusterVersion,
                             "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul,
                                    unpacked->command->header->clusterVersion,
                                    "Command header cluster version invalid");

    // Free dynamically allocated memory
    KineticProto__free_unpacked(unpacked, NULL);
}
#endif
