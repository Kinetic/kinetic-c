#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <protobuf-c/protobuf-c.h>
#include "KineticProto.h"
#include "unity.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticProto_should_pack_and_unpack_protocol_buffers(void)
{
	size_t i, j;
    size_t len, len1, len2;
    KineticProto proto = KINETIC_PROTO_INIT;
    KineticProto* pproto;
    KineticProto_Command cmd = KINETIC_PROTO_COMMAND__INIT;
    KineticProto_Header header = KINETIC_PROTO_HEADER__INIT;
    uint8_t buffer[1024];
    ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT(buffer);
    uint8_t* packed;

    KineticProto_init(&proto);
    KineticProto_command__init(&cmd);
    KineticProto_header__init(&header);

    header.clusterversion = 12345678;
    header.has_clusterversion = TRUE;
    header.identity = -12345678;
    header.has_identity = TRUE;
    cmd.header = &header;
    proto.command = &cmd;

    len = KineticProto_get_packed_size(&proto);
    packed = malloc(KineticProto_get_packed_size(&proto));
    TEST_ASSERT_NOT_NULL_MESSAGE(packed, "Failed dynamically allocating buffer");
    len1 = KineticProto_pack(&proto, packed);
    TEST_ASSERT_EQUAL_MESSAGE(len, len1, "Packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(len, KineticProto_get_packed_size(&proto), "Inspected packed size invalid");

    len2 = KineticProto_pack_to_buffer(&proto, &bs.base);
    TEST_ASSERT_EQUAL_MESSAGE(len, len2, "SimpleBuffer packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(len, bs.len, "SimpleBuffer 'len' packed size invalid");
    TEST_ASSERT_EQUAL_MESSAGE(len, KineticProto_get_packed_size(&proto), "SimpleBuffer inspected packed size invalid");

    // Validate the both packed buffers are equal
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bs.data, packed, len, "Packed proto buffers do not match");
    PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&bs);

    // Unpack and verify the simple buffer results
    pproto = KineticProto_unpack(NULL, len, bs.data);
    TEST_ASSERT_NOT_NULL_MESSAGE(pproto, "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(pproto->command->header->has_identity, "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul, pproto->command->header->identity, "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(pproto->command->header->has_clusterversion, "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul, pproto->command->header->clusterversion, "Command header cluster version invalid");

    // Unpack and verify the raw buffer results
    pproto = KineticProto_unpack(NULL, len, packed);
    TEST_ASSERT_NOT_NULL_MESSAGE(pproto, "Unpack method returned NULL");
    TEST_ASSERT_TRUE_MESSAGE(pproto->command->header->has_identity, "Command header identity reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(-12345678ul, pproto->command->header->identity, "Command header identity invalid");
    TEST_ASSERT_TRUE_MESSAGE(pproto->command->header->has_clusterversion, "Command header cluster version reported as unavailable");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(12345678ul, pproto->command->header->clusterversion, "Command header cluster version invalid");

    // Free dynamically allocated memory
    KineticProto_free_unpacked(pproto, NULL);
    free(packed);
}
