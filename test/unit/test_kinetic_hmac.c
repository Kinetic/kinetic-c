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
#include "kinetic_proto.h"
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic_message.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "zlog/zlog.h"
#include "protobuf-c/protobuf-c.h"
#include <string.h>
#include <openssl/hmac.h>

void setUp(void)
{
    KineticLogger_Init("stdout");
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticHMAC_KINETIC_HMAC_SHA1_LEN_should_be_20(void)
{
    TEST_ASSERT_EQUAL_SIZET(20, KINETIC_HMAC_MAX_LEN);
}

void test_KineticHMAC_KINETIC_HMAC_MAX_LEN_should_be_set_to_proper_maximum_size(void)
{
    TEST_ASSERT_EQUAL_SIZET(KINETIC_HMAC_MAX_LEN, KINETIC_HMAC_MAX_LEN);
}

void test_KineticHMAC_Init_should_initialize_the_HMAC_struct_with_the_specified_algorithm(void)
{
    KineticHMAC actual, expected;
    const KineticProto_Command_Security_ACL_HMACAlgorithm expectedAlgorithm =
        KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1;

    memset(&actual, 0xC5, sizeof(KineticHMAC)); // Initialize with bogus data
    expected.algorithm = expectedAlgorithm;
    memset(expected.data, 0, KINETIC_HMAC_MAX_LEN); // Expect HMAC zeroed out

    KineticHMAC_Init(&actual, expectedAlgorithm);

    TEST_ASSERT_EQUAL(expected.algorithm, actual.algorithm);
    TEST_ASSERT_EQUAL_SIZET(KINETIC_HMAC_MAX_LEN, actual.len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data, actual.data, KINETIC_HMAC_MAX_LEN);
}

void test_KineticHMAC_Init_should_set_HMAC_algorithm_to_INVALID_if_the_specified_algorithm_is_invalid(void)
{
    KineticHMAC actual;

    // Set to valid value to ensure result actually changed
    actual.algorithm = KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1;

    KineticHMAC_Init(&actual, (KineticProto_Command_Security_ACL_HMACAlgorithm)100);

    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_INVALID_HMAC_ALGORITHM, actual.algorithm);
}

void test_KineticHMAC_Populate_should_compute_and_populate_the_SHA1_HMAC_for_the_supplied_message_and_key(void)
{
    KineticHMAC actual;
    KineticProto_Message msg = KINETIC_PROTO_MESSAGE__INIT;
    KineticProto_Message_HMACauth hmacAuth = KINETIC_PROTO_MESSAGE_HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    uint8_t commandBytes[123];
    ByteArray commandArray = ByteArray_Create(commandBytes, sizeof(commandBytes));
    ByteArray_FillWithDummyData(commandArray);
    ProtobufCBinaryData dummyCommandData = {.data = commandArray.data, .len = commandArray.len};

    msg.commandBytes = dummyCommandData;
    msg.has_commandBytes = true;
    msg.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    msg.has_authType = true;
    hmacAuth.has_hmac = true;
    hmacAuth.hmac = hmac;
    msg.hmacAuth = &hmacAuth;

    KineticHMAC_Init(&actual, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&actual, &msg, key);

    TEST_ASSERT_TRUE(msg.hmacAuth->has_hmac);
    TEST_ASSERT_EQUAL_PTR(hmac.data, msg.hmacAuth->hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_MAX_LEN, msg.hmacAuth->hmac.len);

    LOG("Computed HMAC: ");
    LOGF("  %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
         actual.data[0],  actual.data[1],  actual.data[2],  actual.data[3],
         actual.data[4],  actual.data[5],  actual.data[6],  actual.data[7]);
    LOGF("  %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
         actual.data[8],  actual.data[9],  actual.data[10], actual.data[11],
         actual.data[12], actual.data[13], actual.data[14], actual.data[15]);
    LOGF("  %02hhX%02hhX%02hhX%02hhX",
         actual.data[16], actual.data[17], actual.data[18], actual.data[19]);
}

void test_KineticHMAC_Validate_should_return_true_if_the_HMAC_for_the_supplied_message_and_key_is_correct(void)
{
    KineticHMAC actual;
    KineticProto_Command_Status status = KINETIC_PROTO_COMMAND_STATUS__INIT;
    KineticProto_Command command = KINETIC_PROTO_COMMAND__INIT;
    status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE;
    status.has_code = true;
    command.status = &status;
    KineticProto_Message proto = KINETIC_PROTO_MESSAGE__INIT;
    KineticProto_Message_HMACauth hmacAuth = KINETIC_PROTO_MESSAGE_HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandBytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = KineticProto_command__pack(&command, packedCmd);
    proto.commandBytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacAuth = &hmacAuth;
    proto.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    proto.has_authType = true;

    KineticHMAC_Init(&actual, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_value_of_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    KineticProto_Command_Status status = KINETIC_PROTO_COMMAND_STATUS__INIT;
    KineticProto_Command command = KINETIC_PROTO_COMMAND__INIT;
    status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE;
    status.has_code = true;
    command.status = &status;
    KineticProto_Message proto = KINETIC_PROTO_MESSAGE__INIT;
    KineticProto_Message_HMACauth hmacAuth = KINETIC_PROTO_MESSAGE_HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandBytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = KineticProto_command__pack(&command, packedCmd);
    proto.commandBytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacAuth = &hmacAuth;
    proto.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    proto.has_authType = true;

    KineticHMAC_Init(&actual, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.hmac.data[3]++;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_length_of_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    KineticProto_Command_Status status = KINETIC_PROTO_COMMAND_STATUS__INIT;
    KineticProto_Command command = KINETIC_PROTO_COMMAND__INIT;
    status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE;
    status.has_code = true;
    command.status = &status;
    KineticProto_Message proto = KINETIC_PROTO_MESSAGE__INIT;
    KineticProto_Message_HMACauth hmacAuth = KINETIC_PROTO_MESSAGE_HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandBytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = KineticProto_command__pack(&command, packedCmd);
    proto.commandBytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacAuth = &hmacAuth;
    proto.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    proto.has_authType = true;

    KineticHMAC_Init(&actual, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.hmac.len--;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_presence_is_false_for_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    KineticProto_Command_Status status = KINETIC_PROTO_COMMAND_STATUS__INIT;
    KineticProto_Command command = KINETIC_PROTO_COMMAND__INIT;
    status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE;
    status.has_code = true;
    command.status = &status;
    KineticProto_Message proto = KINETIC_PROTO_MESSAGE__INIT;
    KineticProto_Message_HMACauth hmacAuth = KINETIC_PROTO_MESSAGE_HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandBytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = KineticProto_command__pack(&command, packedCmd);
    proto.commandBytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacAuth = &hmacAuth;
    proto.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    proto.has_authType = true;

    KineticHMAC_Init(&actual, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.has_hmac = false;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}
