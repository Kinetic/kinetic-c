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

#include "unity_helper.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_request.h"

#include "kinetic_logger.h"
#include "byte_array.h"

#include "mock_kinetic_auth.h"
#include "mock_kinetic_nbo.h"
#include "mock_bus.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_countingsemaphore.h"
#include "mock_kinetic_proto.h"

extern uint8_t *cmdBuf;
extern uint8_t *msg;

void setUp(void)
{
    KineticLogger_Init("stdout", 1);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticRequest_PackCommand_should_return_KINETIC_REQUEST_PACK_FAILURE_on_malloc_fail(void)
{
    KineticMessage message;
    memset(&message, 0, sizeof(message));
    KineticRequest request;
    memset(&request, 0, sizeof(request));
    request.message = message;

    KineticProto_command__get_packed_size_ExpectAndReturn(&request.message.command,
        ((size_t)-1));

    cmdBuf = NULL;  // fake malloc failure
    TEST_ASSERT_EQUAL(KINETIC_REQUEST_PACK_FAILURE, KineticRequest_PackCommand(&request));
}

void test_KineticRequest_PackCommand_should_return_size_when_packing_command(void)
{
    KineticMessage message;
    memset(&message, 0, sizeof(message));
    KineticRequest request;
    memset(&request, 0, sizeof(request));
    request.message = message;

    uint8_t buf[12345];
    cmdBuf = buf;
    KineticProto_command__get_packed_size_ExpectAndReturn(&request.message.command,
        (size_t)12345);

    KineticProto_command__pack_ExpectAndReturn(&request.message.command,
        cmdBuf, 12345);
    TEST_ASSERT_EQUAL(12345, KineticRequest_PackCommand(&request));

    TEST_ASSERT_EQUAL(12345, request.message.message.commandBytes.len);
    TEST_ASSERT_TRUE(request.message.message.has_commandBytes);
}

void test_KineticRequest_PopulateAuthentication_should_use_PIN_if_provided(void)
{
    KineticSessionConfig config;
    KineticRequest request;
    ByteArray pin;

    KineticAuth_PopulatePin_ExpectAndReturn(&config, &request, pin, KINETIC_STATUS_SUCCESS);
    KineticStatus res = KineticRequest_PopulateAuthentication(&config, &request, &pin);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, res);
}

void test_KineticRequest_PopulateAuthentication_should_use_HMAC_if_PIN_is_NULL(void)
{
    KineticSessionConfig config;
    KineticRequest request;

    KineticAuth_PopulateHmac_ExpectAndReturn(&config, &request, KINETIC_STATUS_SUCCESS);
    KineticStatus res = KineticRequest_PopulateAuthentication(&config, &request, NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, res);
}

void test_KineticRequest_PackMessage_should_return_MEMORY_ERROR_on_alloc_fail(void)
{
    KineticRequest request;
    KineticOperation operation = {
        .request = &request,
        .value.len = 9999,
    };

    uint8_t *out_msg = NULL;
    size_t msgSize = 0;

    KineticProto_Message* proto = &operation.request->message.message;
    KineticProto_Message__get_packed_size_ExpectAndReturn(proto, 12345);

    KineticNBO_FromHostU32_ExpectAndReturn(12345, 0xaabbccdd);
    KineticNBO_FromHostU32_ExpectAndReturn(9999, 0xddccbbaa);

    msg = NULL;  // fake malloc failure
    KineticStatus status = KineticRequest_PackMessage(&operation, &out_msg, &msgSize);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_MEMORY_ERROR, status);
}

void test_KineticRequest_PackMessage_should_return_SUCCESS_on_message_packing(void)
{
    KineticRequest request;
    memset(&request, 0, sizeof(request));

    size_t valueLen = 5;
    size_t packedSize = 6 + 16;  // mostly for nice alignment in hexdump

    KineticOperation operation = {
        .request = &request,
        .value.len = valueLen,
    };

    uint8_t valueBuf[] = "value";
    operation.value.data = valueBuf;

    uint8_t *out_msg = NULL;
    size_t msgSize = 0;

    KineticProto_Message* proto = &operation.request->message.message;
    KineticProto_Message__get_packed_size_ExpectAndReturn(proto, packedSize);

    // size of filler protobuf, mainly to get things nicely aligned w/ 9-byte header
    KineticNBO_FromHostU32_ExpectAndReturn(packedSize, 0xaabbccdd);

    KineticNBO_FromHostU32_ExpectAndReturn(valueLen, 0xddccbbaa);

    uint8_t buf[64 * 1024];
    memset(buf, 0, sizeof(buf));
    msg = &buf[0];  // fake malloc
    size_t offset = sizeof(uint8_t) + 2*sizeof(uint32_t);

    // filler where protobuf message would be
    for (int i = 0; i < packedSize; i++) { buf[i + offset] = 0x33; }

    KineticProto_Message__pack_ExpectAndReturn(&request.message.message, &buf[offset], packedSize);

    KineticStatus status = KineticRequest_PackMessage(&operation, &out_msg, &msgSize);
    TEST_ASSERT_EQUAL(out_msg, msg);
    TEST_ASSERT(msgSize > 0);

    #if 0
    for (size_t i = 0; i < msgSize; i++) {
        if (i > 0 && (i & 15) == 15) { printf("\n"); }
        printf("%02x ", out_msg[i]);
    }
    printf("\n");
    #endif

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    uint8_t expected[] = {
        'F',  // versionPrefix
        0xdd, 0xcc, 0xbb, 0xaa,  // proto length
        0xaa, 0xbb, 0xcc, 0xdd,  // value length
    };
    for (int i = 0; i < sizeof(expected); i++) {
        TEST_ASSERT_EQUAL(expected[i], out_msg[i]);
    }
    for (int i = 0; i < packedSize; i++) {
        TEST_ASSERT_EQUAL(0x33, out_msg[i + offset]);
    }
    for (int i = 0; i < valueLen; i++) {
        TEST_ASSERT_EQUAL(valueBuf[i], out_msg[i + offset + packedSize]);
    }
}
