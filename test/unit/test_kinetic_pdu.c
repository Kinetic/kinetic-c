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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_pdu.h"
#include "kinetic_nbo.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"
#include "protobuf-c/protobuf-c.h"
#include <arpa/inet.h>
#include <string.h>

static KineticPDU PDU;
static KineticConnection Connection;
static int64_t Identity = 1234;
static ByteArray Key;
static uint8_t ValueBuffer[PDU_VALUE_MAX_LEN];
static ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void setUp(void)
{
    // Create and configure a new Kinetic protocol instance
    KINETIC_CONNECTION_INIT(&Connection, Identity, Key);
    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");
    KINETIC_PDU_INIT(&PDU, &Connection);
    BYTE_ARRAY_FILL_WITH_DUMMY_DATA(Value);
    KineticLogger_Init(NULL);
    Key = BYTE_ARRAY_INIT_FROM_CSTRING("some valid HMAC key...");
}

void tearDown(void)
{
}

void test_KineticPDUHeader_should_have_correct_byte_packed_size(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1+4+4, PDU_HEADER_LEN);
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN, sizeof(KineticPDUHeader));
}

void test_KineteicPDU_PDU_PROTO_MAX_LEN_should_be_1MB(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1024*1024, PDU_PROTO_MAX_LEN);
}

void test_KineteicPDU_PDU_VALUE_MAX_LEN_should_be_1MB(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1024*1024, PDU_VALUE_MAX_LEN);
}

void test_KineticPDU_PDU_VALUE_MAX_LEN_should_be_the_sum_of_header_protobuf_and_value_max_lengths(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN, PDU_MAX_LEN);
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer(void)
{
    LOG_LOCATION;
    PDU.value = (ByteArray){.data = (uint8_t*)0xDEADBEEF, .len = 27453};

    KineticPDU_Init(&PDU, &Connection);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Connection, PDU.connection);
}

void test_KineticPDU_Init_should_set_the_exchange_fields_in_the_embedded_protobuf_header(void)
{
    LOG_LOCATION;
    KINETIC_CONNECTION_INIT(&Connection, 12, BYTE_ARRAY_NONE);
    Connection.identity = 12;
    Connection.sequence = 24;
    Connection.clusterVersion = 1122334455667788;
    Connection.identity = 37;
    Connection.connectionID = 8765432;
    KINETIC_PDU_INIT(&PDU, &Connection);

    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788,
        PDU.protoData.message.header.clusterVersion);
    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_identity);
    TEST_ASSERT_EQUAL_INT64(37, PDU.protoData.message.header.identity);
    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_connectionID);
    TEST_ASSERT_EQUAL_INT64(8765432, PDU.protoData.message.header.connectionID);
    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(24, PDU.protoData.message.header.sequence);
}



void test_KineticPDU_AttachValuePayload_should_attach_specified_ByteArray(void)
{
    LOG_LOCATION;
    KineticPDU_Init(&PDU, &Connection);

    uint8_t data[5] = {5,4,3,2,1};
    const ByteArray payload = {.data = data, .len = sizeof(data)};

    KineticPDU_AttachValuePayload(&PDU, payload);

    TEST_ASSERT_EQUAL_ByteArray(payload, PDU.value);
    TEST_ASSERT_EQUAL_PTR(data, PDU.value.data);
    TEST_ASSERT_EQUAL(sizeof(data), PDU.value.len);
}



void test_KineticPDU_EnableValueBuffer_should_attach_builtin_buffer(void)
{
    LOG_LOCATION;
    KineticPDU_Init(&PDU, &Connection);

    KineticPDU_EnableValueBuffer(&PDU);

    TEST_ASSERT_EQUAL_PTR(PDU.valueBuffer, PDU.value.data);
    TEST_ASSERT_EQUAL(PDU_VALUE_MAX_LEN, PDU.value.len);
}



void test_KineticPDU_EnableValueBuffer_should_attach_builtin_buffer_and_set_length(void)
{
    LOG_LOCATION;
    KineticPDU_Init(&PDU, &Connection);
    PDU.valueBuffer[0] = 0xAA;
    PDU.valueBuffer[1] = 0xBB;
    PDU.valueBuffer[2] = 0xCC;
    ByteArray expected = {.data = PDU.valueBuffer, .len = 3};

    KineticPDU_EnableValueBufferWithLength(&PDU, expected.len);

    TEST_ASSERT_EQUAL_PTR(PDU.valueBuffer, PDU.value.data);
    TEST_ASSERT_EQUAL(3, PDU.value.len);
    TEST_ASSERT_EQUAL_UINT32(3, PDU.header.valueLength);
    TEST_ASSERT_EQUAL_uint32_nbo_t(3, PDU.headerNBO.valueLength);
    TEST_ASSERT_EQUAL_HEX8(0xAA, PDU.value.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, PDU.value.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, PDU.value.data[2]);
}

// Disabled, since uses an assert() instead. Re-enable test if asserts are converted to exceptions!
// void test_KineticPDU_EnableValueBuffer_should_attach_builtin_buffer_and_reject_lengths_that_are_too_long(void)
// {
//     LOG_LOCATION;
//     KineticPDU_Init(&PDU, &Connection);

//     size_t actualLength = KineticPDU_EnableValueBufferWithLength(&PDU, PDU_VALUE_MAX_LEN+1);

//     TEST_ASSERT_EQUAL(0, actualLength);
//     TEST_ASSERT_ByteArray_NONE(PDU.value);
// }


void test_KINETIC_PDU_INIT_WITH_MESSAGE_should_initialize_PDU_and_protobuf_message(void)
{
    LOG_LOCATION;
    memset(&PDU, 0, sizeof(KineticPDU));
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);

    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY;
    PDU.protoData.message.command.status = &PDU.protoData.message.status;

    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY,
        PDU.proto->command->status->code);
}


void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_no_value_payload(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_value_payload(void)
{
    LOG_LOCATION;
    bool status;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("Some arbitrary value");

    KineticPDU_Init(&PDU, &Connection);
    KineticPDU_AttachValuePayload(&PDU, value);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, PDU.value, true);

    status = KineticPDU_Send(&PDU);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_embedded_value_payload(void)
{
    LOG_LOCATION;
    bool status;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    PDU.valueBuffer[0] = 0xDE;
    PDU.valueBuffer[1] = 0x4A;
    PDU.valueBuffer[2] = 0xCE;
    KineticPDU_EnableValueBuffer(&PDU);
    PDU.value.len = 3;
    ByteArray expectedValue = {.data = PDU.value.data, .len = 3};

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, expectedValue, true);

    status = KineticPDU_Send(&PDU);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_header(void)
{
    LOG_LOCATION;
    bool status;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};

    KineticPDU_Init(&PDU, &Connection);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, false);

    status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_protobuf(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, false);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_if_value_write_fails(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray headerNBO = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    uint8_t valueBuffer[10];
    ByteArray value = {.data = valueBuffer, .len = sizeof(valueBuffer)};

    KineticPDU_AttachValuePayload(&PDU, value);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->key);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, value, false);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}



void test_KineticPDU_Receive_should_receive_a_message_with_value_payload_and_return_true_upon_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteArray expectedPDUHeaderArray = {
        .data = (uint8_t*)&PDU.headerNBO,
        .len = sizeof(KineticPDUHeader),
    };

    // Fake value/payload length
    KineticPDU_EnableValueBuffer(&PDU);
    ByteArray expectedValue = {.data = PDU.valueBuffer, .len = 124};
    BYTE_ARRAY_FILL_WITH_DUMMY_DATA(expectedValue);
    KineticPDU_AttachValuePayload(&PDU, expectedValue);

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
        expectedPDUHeaderArray, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor,
        &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
        expectedValue, true);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);

    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, PDU.protoData.message.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_successful_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    KineticPDU_AttachValuePayload(&PDU, BYTE_ARRAY_NONE);

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, true);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS,
        PDU.protoData.message.status.code);
    TEST_ASSERT_ByteArray_NONE(PDU.value);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_receipt_of_valid_PDU_with_non_successful_status(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    KineticPDU_EnableValueBuffer(&PDU);
    ByteArray enabledValue = {.data = PDU.valueBuffer, .len = PDU_VALUE_MAX_LEN};
    TEST_ASSERT_EQUAL_ByteArray(enabledValue, PDU.value);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, true);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR,
        PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL_PTR(PDU.valueBuffer, PDU.value.data);
    TEST_ASSERT_EQUAL(0, PDU.value.len);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_receipt_of_valid_PDU_with_non_successful_status_for_provided_value_buffer(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    uint8_t valueData[64];
    ByteArray attachedValue = {.data = valueData, .len = sizeof(valueData)};
    KineticPDU_AttachValuePayload(&PDU, attachedValue);
    TEST_ASSERT_EQUAL_ByteArray(attachedValue, PDU.value);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, true);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR,
        PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL_PTR(valueData, PDU.value.data);
    TEST_ASSERT_EQUAL(0, PDU.value.len);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_header(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, false);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_protobuf(void)
{
    LOG_LOCATION;
    bool status;

    KineticPDU_Init(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;

    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    ByteArray protobuf = {.data = PDU.protobufRaw, .len = 12};
    PDU.headerNBO = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
        .protobufLength = KineticNBO_FromHostU32(protobuf.len),
        .valueLength = 0
    };

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, false);

    status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_HMAC_validation_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    KineticPDU_AttachValuePayload(&PDU, BYTE_ARRAY_NONE);

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, false);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_value_field_receive_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteArray header = {.data = (uint8_t*)&PDU.headerNBO, .len = sizeof(KineticPDUHeader)};
    PDU.headerNBO = (KineticPDUHeader){
        .versionPrefix = 'F',
        .protobufLength = KineticNBO_ToHostU32(17),
        .valueLength = KineticNBO_ToHostU32(124),
    };

    // Fake value/payload length
    KineticPDU_EnableValueBuffer(&PDU);
    ByteArray expectedValue = {.data = PDU.valueBuffer, .len = 124};
    BYTE_ARRAY_FILL_WITH_DUMMY_DATA(expectedValue);
    KineticPDU_AttachValuePayload(&PDU, expectedValue);

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, header, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->key, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, expectedValue, false);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
    TEST_ASSERT_EQUAL(17, PDU.header.protobufLength);
    TEST_ASSERT_EQUAL(expectedValue.len, PDU.header.valueLength);
}
