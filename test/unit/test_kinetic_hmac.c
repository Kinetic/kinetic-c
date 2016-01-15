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

#include "unity_helper.h"
#include "kinetic.pb-c.h"
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic_message.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include <string.h>
#include <openssl/hmac.h>

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
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
    const Com__Seagate__Kinetic__Proto__Command__Security__ACL__HMACAlgorithm expectedAlgorithm =
        COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1;

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
    actual.algorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1;

    KineticHMAC_Init(&actual, (Com__Seagate__Kinetic__Proto__Command__Security__ACL__HMACAlgorithm)100);

    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__INVALID_HMAC_ALGORITHM, actual.algorithm);
}

void test_KineticHMAC_Populate_should_compute_and_populate_the_SHA1_HMAC_for_the_supplied_message_and_key(void)
{
    KineticHMAC actual;
    Com__Seagate__Kinetic__Proto__Message msg = COM__SEAGATE__KINETIC__PROTO__MESSAGE__INIT;
    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth = COM__SEAGATE__KINETIC__PROTO__MESSAGE__HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    uint8_t commandBytes[123];
    ByteArray commandArray = ByteArray_Create(commandBytes, sizeof(commandBytes));
    ByteArray_FillWithDummyData(commandArray);
    ProtobufCBinaryData dummyCommandData = {.data = commandArray.data, .len = commandArray.len};

    msg.commandbytes = dummyCommandData;
    msg.has_commandbytes = true;
    msg.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    msg.has_authtype = true;
    hmacAuth.has_hmac = true;
    hmacAuth.hmac = hmac;
    msg.hmacauth = &hmacAuth;

    KineticHMAC_Init(&actual, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&actual, &msg, key);

    TEST_ASSERT_TRUE(msg.hmacauth->has_hmac);
    TEST_ASSERT_EQUAL_PTR(hmac.data, msg.hmacauth->hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_MAX_LEN, msg.hmacauth->hmac.len);

    LOG0("Computed HMAC: ");
    LOGF0("  %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
         actual.data[0],  actual.data[1],  actual.data[2],  actual.data[3],
         actual.data[4],  actual.data[5],  actual.data[6],  actual.data[7]);
    LOGF0("  %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
         actual.data[8],  actual.data[9],  actual.data[10], actual.data[11],
         actual.data[12], actual.data[13], actual.data[14], actual.data[15]);
    LOGF0("  %02hhX%02hhX%02hhX%02hhX",
         actual.data[16], actual.data[17], actual.data[18], actual.data[19]);
}

void test_KineticHMAC_Validate_should_return_true_if_the_HMAC_for_the_supplied_message_and_key_is_correct(void)
{
    KineticHMAC actual;
    Com__Seagate__Kinetic__Proto__Command__Status status = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__INIT;
    Com__Seagate__Kinetic__Proto__Command command = COM__SEAGATE__KINETIC__PROTO__COMMAND__INIT;
    status.code = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE;
    status.has_code = true;
    command.status = &status;
    Com__Seagate__Kinetic__Proto__Message proto = COM__SEAGATE__KINETIC__PROTO__MESSAGE__INIT;
    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth = COM__SEAGATE__KINETIC__PROTO__MESSAGE__HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandbytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = com__seagate__kinetic__proto__command__pack(&command, packedCmd);
    proto.commandbytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacauth = &hmacAuth;
    proto.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    proto.has_authtype = true;

    KineticHMAC_Init(&actual, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_value_of_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    Com__Seagate__Kinetic__Proto__Command__Status status = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__INIT;
    Com__Seagate__Kinetic__Proto__Command command = COM__SEAGATE__KINETIC__PROTO__COMMAND__INIT;
    status.code = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE;
    status.has_code = true;
    command.status = &status;
    Com__Seagate__Kinetic__Proto__Message proto = COM__SEAGATE__KINETIC__PROTO__MESSAGE__INIT;
    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth = COM__SEAGATE__KINETIC__PROTO__MESSAGE__HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandbytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = com__seagate__kinetic__proto__command__pack(&command, packedCmd);
    proto.commandbytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacauth = &hmacAuth;
    proto.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    proto.has_authtype = true;

    KineticHMAC_Init(&actual, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.hmac.data[3]++;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_length_of_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    Com__Seagate__Kinetic__Proto__Command__Status status = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__INIT;
    Com__Seagate__Kinetic__Proto__Command command = COM__SEAGATE__KINETIC__PROTO__COMMAND__INIT;
    status.code = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE;
    status.has_code = true;
    command.status = &status;
    Com__Seagate__Kinetic__Proto__Message proto = COM__SEAGATE__KINETIC__PROTO__MESSAGE__INIT;
    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth = COM__SEAGATE__KINETIC__PROTO__MESSAGE__HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandbytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = com__seagate__kinetic__proto__command__pack(&command, packedCmd);
    proto.commandbytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacauth = &hmacAuth;
    proto.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    proto.has_authtype = true;

    KineticHMAC_Init(&actual, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.hmac.len--;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}

void test_KineticHMAC_Validate_should_return_false_if_the_HMAC_presence_is_false_for_the_supplied_message_and_key_is_incorrect(void)
{
    KineticHMAC actual;
    Com__Seagate__Kinetic__Proto__Command__Status status = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__INIT;
    Com__Seagate__Kinetic__Proto__Command command = COM__SEAGATE__KINETIC__PROTO__COMMAND__INIT;
    status.code = COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE;
    status.has_code = true;
    command.status = &status;
    Com__Seagate__Kinetic__Proto__Message proto = COM__SEAGATE__KINETIC__PROTO__MESSAGE__INIT;
    Com__Seagate__Kinetic__Proto__Message__HMACauth hmacAuth = COM__SEAGATE__KINETIC__PROTO__MESSAGE__HMACAUTH__INIT;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
    ProtobufCBinaryData hmac = {.len = KINETIC_HMAC_MAX_LEN, .data = data};
    const ByteArray key = ByteArray_CreateWithCString("1234567890ABCDEFGHIJK");
    proto.has_commandbytes = true;
    uint8_t packedCmd[128];
    size_t packedLen = com__seagate__kinetic__proto__command__pack(&command, packedCmd);
    proto.commandbytes = (ProtobufCBinaryData){.data = packedCmd, .len = packedLen};
    hmacAuth.identity = 7;
    hmacAuth.has_identity = true;
    hmacAuth.hmac = hmac;
    hmacAuth.has_hmac = true;
    proto.hmacauth = &hmacAuth;
    proto.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    proto.has_authtype = true;

    KineticHMAC_Init(&actual, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&actual, &proto, key);

    TEST_ASSERT_TRUE(KineticHMAC_Validate(&proto, key));

    // Bork the HMAC
    hmacAuth.has_hmac = false;

    TEST_ASSERT_FALSE(KineticHMAC_Validate(&proto, key));
}
