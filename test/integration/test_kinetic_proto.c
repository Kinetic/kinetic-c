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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <protobuf-c/protobuf-c.h>
#include "kinetic_types.h"
#include "kinetic_proto.h"
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
    KineticProto proto;
    KineticProto* unpacked;
    KineticProto_Command cmd;
    KineticProto_Header header;
    uint8_t buffer[1024];
    ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT(buffer);
    uint8_t* packed;

    KineticProto_init(&proto);
    KineticProto_command_init(&cmd);
    KineticProto_header_init(&header);

    cmd.header = &header;
    proto.command = &cmd;

    expectedLen = KineticProto_get_packed_size(&proto);
    packed = malloc(KineticProto_get_packed_size(&proto));
    TEST_ASSERT_NOT_NULL_MESSAGE(packed, "Failed dynamically allocating buffer");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen, KineticProto_pack(&proto, packed), "Packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen, KineticProto_get_packed_size(&proto), "Inspected packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(expectedLen, KineticProto_pack_to_buffer(&proto, &bs.base), "Packed BufferSimple size invalid");

    // Validate the both packed buffers are equal
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bs.data, packed, expectedLen, "Packed proto buffers do not match");
    PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&bs);

    // Free dynamically allocated memory
    free(packed);
}

void test_KineticProto_should_pass_data_accurately_through_raw_buffers(void)
{
    uint8_t* packed;
    KineticProto* unpacked;
    KineticProto proto;
    KineticProto_Command cmd;
    KineticProto_Header header;

    KineticProto_init(&proto);
    KineticProto_command_init(&cmd);
    proto.command = &cmd;
    KineticProto_header_init(&header);
    header.clusterversion = 12345678;
    header.has_clusterversion = true;
    header.identity = -12345678;
    header.has_identity = true;
    cmd.header = &header;

    // Pack the buffer
    packed = malloc(KineticProto_get_packed_size(&proto));
    KineticProto_pack(&proto, packed);

    // Unpack and verify the raw buffer results
    unpacked = KineticProto_unpack(NULL, KineticProto_get_packed_size(&proto), packed);
    TEST_ASSERT_NOT_NULL_MESSAGE(unpacked, "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_identity, "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul, unpacked->command->header->identity, "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_clusterversion, "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul, unpacked->command->header->clusterversion, "Command header cluster version invalid");

    // Free dynamically allocated memory
    KineticProto_free_unpacked(unpacked, NULL);
    free(packed);
}

void test_KineticProto_should_pass_data_accurately_through_BufferSimple(void)
{
    KineticProto proto;
    KineticProto* unpacked;
    KineticProto_Command cmd;
    KineticProto_Header header;
    uint8_t buffer[1024];
    ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT(buffer);

    KineticProto_init(&proto);
    KineticProto_command_init(&cmd);
    proto.command = &cmd;
    KineticProto_header_init(&header);
    header.clusterversion = 12345678;
    header.has_clusterversion = true;
    header.identity = -12345678;
    header.has_identity = true;
    cmd.header = &header;

    // Pack the buffer
    KineticProto_pack_to_buffer(&proto, &bs.base);

    // Unpack and verify the simple buffer results
    unpacked = KineticProto_unpack(NULL, KineticProto_get_packed_size(&proto), bs.data);
    TEST_ASSERT_NOT_NULL_MESSAGE(unpacked, "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_identity, "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul, unpacked->command->header->identity, "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(unpacked->command->header->has_clusterversion, "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul, unpacked->command->header->clusterversion, "Command header cluster version invalid");

    // Free dynamically allocated memory
    KineticProto_free_unpacked(unpacked, NULL);
}
