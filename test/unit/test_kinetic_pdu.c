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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_pdu.h"
#include "kinetic_nbo.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include <arpa/inet.h>
#include <string.h>

static KineticPDU PDU;
static KineticConnection Connection;
static KineticSession Session;
static ByteArray Key;
static uint8_t ValueBuffer[PDU_VALUE_MAX_LEN];
static ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void setUp(void)
{
    // Create and configure a new Kinetic protocol instance
    Key = ByteArray_CreateWithCString("some valid HMAC key...");
    Session = (KineticSession) {
        .nonBlocking = false,
         .port = 1234,
          .host = "valid-host.com",
        .hmacKey = (ByteArray) {.data = &Session.keyData[0], .len = Key.len},
    };
    memcpy(Session.hmacKey.data, Key.data, Key.len);

    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = true;
    Connection.socket = 456;
    Connection.session = Session;

    KINETIC_PDU_INIT(&PDU, &Connection);
    ByteArray_FillWithDummyData(Value);
    KineticLogger_Init(NULL);
}

void test_KineticPDUHeader_should_have_correct_byte_packed_size(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1 + 4 + 4, PDU_HEADER_LEN);
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN, sizeof(KineticPDUHeader));
}

void test_KineteicPDU_PDU_PROTO_MAX_LEN_should_be_1MB(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1024 * 1024, PDU_PROTO_MAX_LEN);
}

void test_KineteicPDU_PDU_VALUE_MAX_LEN_should_be_1MB(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1024 * 1024, PDU_VALUE_MAX_LEN);
}

void test_KineticPDU_PDU_VALUE_MAX_LEN_should_be_the_sum_of_header_protobuf_and_value_max_lengths(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN, PDU_MAX_LEN);
}


void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer(void)
{
    LOG_LOCATION;

    KineticPDU_Init(&PDU, &Connection);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Connection, PDU.connection);
}

void test_KineticPDU_Init_should_set_the_exchange_fields_in_the_embedded_protobuf_header(void)
{
    LOG_LOCATION;
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.sequence = 24;
    Connection.connectionID = 8765432;
    Session = (KineticSession) {
        .clusterVersion = 1122334455667788,
         .identity = 37,
    };
    Connection.session = Session;
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


void test_KineticPDU_AttachEntry_should_attach_specified_KineticEntry_with_new_buffer(void)
{
    LOG_LOCATION;
    KineticPDU_Init(&PDU, &Connection);

    uint8_t data[5];
    ByteArray value = {.data = data, .len = sizeof(data)};
    ByteBuffer valueBuffer = ByteBuffer_CreateWithArray(value);
    KineticEntry entry = {.value = valueBuffer};
    entry.value.bytesUsed = 3;

    KineticPDU_AttachEntry(&PDU, &entry);

    TEST_ASSERT_EQUAL_PTR(entry.value.array.data, PDU.entry.value.array.data);
    TEST_ASSERT_EQUAL(0, PDU.entry.value.bytesUsed);
    TEST_ASSERT_EQUAL(3, entry.value.bytesUsed);
}


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
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));
    KineticEntry entry = {.value = BYTE_BUFFER_NONE};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac,
                            KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
                                &PDU.protoData.message.proto, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_value_payload(void)
{
    LOG_LOCATION;
    bool status;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
                                &PDU.protoData.message.proto, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &PDU.entry.value, true);

    status = KineticPDU_Send(&PDU);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_header(void)
{
    LOG_LOCATION;
    bool status;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, false);

    status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_protobuf(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
                                &PDU.protoData.message.proto, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, false);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_if_value_write_fails(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.proto, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &entry.value, false);

    bool status = KineticPDU_Send(&PDU);

    TEST_ASSERT_FALSE(status);
}


void test_KineticPDU_Receive_should_receive_a_message_with_value_payload_and_return_true_upon_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    // Fake value/payload length
    uint8_t data[1024];
    ByteArray expectedValue = ByteArray_Create(data, sizeof(data));
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, true);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    PDU.protoData.message.header.connectionID = 12345;

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS,
                      PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL(12345, PDU.proto->command->header->connectionID);
    TEST_ASSERT_EQUAL(12345, PDU.connection->connectionID);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_successful_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    PDU.protoData.message.header.connectionID = 12345;

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS,
                      PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL(12345, PDU.proto->command->header->connectionID);
    TEST_ASSERT_EQUAL(12345, PDU.connection->connectionID);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_receipt_of_valid_PDU_with_non_successful_status(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    // Fake value/payload length
    uint8_t data[1024];
    ByteArray expectedValue = {.data = data, .len = sizeof(data)};
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR,
        PDU.protoData.message.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_header(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, false);

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

    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));
    ByteArray protobuf = {.data = PDU.protobufRaw, .len = 12};
    PDU.headerNBO = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
         .protobufLength = KineticNBO_FromHostU32(protobuf.len),
          .valueLength = 0
    };

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, false);

    status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_HMAC_validation_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, false);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_value_field_receive_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));
    PDU.headerNBO = (KineticPDUHeader) {
        .versionPrefix = 'F', .protobufLength = KineticNBO_ToHostU32(17),
        .valueLength = KineticNBO_ToHostU32(124)
    };

    // Fake value/payload length
    uint8_t data[124];
    ByteArray expectedValue = ByteArray_Create(data, sizeof(data));
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, false);

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_FALSE(status);
    TEST_ASSERT_EQUAL(17, PDU.header.protobufLength);
    TEST_ASSERT_EQUAL(expectedValue.len, PDU.header.valueLength);
}

void test_KineticPDU_Receive_should_receive_a_message_and_NOT_update_the_ConnectionID_for_the_connection_if_not_provided(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader));

    // Fake value/payload length
    uint8_t data[124];
    ByteArray expectedValue = {.data = data, .len = sizeof(data)};
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, true);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, true);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    PDU.protoData.message.header.connectionID = 12345;

    bool status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_TRUE(status);

    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL(12345, PDU.proto->command->header->connectionID);
    TEST_ASSERT_EQUAL(12345, PDU.connection->connectionID);
}






void test_KineticPDU_GetStatus_should_return_KINETIC_STATUS_INVALID_if_no_KineticProto_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    status = KineticPDU_GetStatus(NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto = NULL;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
    
    PDU.proto = &PDU.protoData.message.proto;
    PDU.proto->command = NULL;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto->command = &PDU.protoData.message.command;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto->command->status = &PDU.protoData.message.status;
    PDU.proto->command->status->has_code = false;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto->command->status->has_code = true;
    PDU.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
}

void test_KineticPDU_GetStatus_should_return_appropriate_KineticStatus_based_on_KineticProto_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    PDU.proto = &PDU.protoData.message.proto;
    PDU.proto->command = &PDU.protoData.message.command;
    PDU.proto->command->status = &PDU.protoData.message.status;
    PDU.proto->command->status->has_code = true;

    PDU.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    PDU.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticPDU_GetKeyValue_should_return_NULL_message_has_no_KeyValue(void)
{
    LOG_LOCATION;

    KineticProto_KeyValue* keyValue;

    PDU.proto = NULL;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);
    
    PDU.proto = &PDU.protoData.message.proto;
    PDU.proto->command = NULL;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.proto->command = &PDU.protoData.message.command;
    PDU.protoData.message.command.body = NULL;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.protoData.message.command.body = &PDU.protoData.message.body;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.protoData.message.command.body->keyValue = &PDU.protoData.message.keyValue;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NOT_NULL(keyValue);
}
