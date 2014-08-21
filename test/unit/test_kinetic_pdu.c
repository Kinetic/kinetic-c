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
#include "kinetic_types.h"
#include "kinetic_pdu.h"
#include "protobuf-c.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"

#include <string.h>

static KineticPDU PDU;
static KineticConnection Connection;
static int64_t Identity = 1234;
static const ByteArray Key = BYTE_ARRAY_INIT_FROM_CSTRING("some valid HMAC key...");

static KineticMessage MessageOut, MessageIn;
static KineticPDU PDUOut, PDUIn;

static int64_t ProtoBufferLength;
static uint8_t ValueBuffer[PDU_VALUE_MAX_LEN];
static const ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void setUp(void)
{
    size_t i;

    // Assemble a Kinetic protocol instance
    KINETIC_CONNECTION_INIT(&Connection, Identity, Key);
    KINETIC_MESSAGE_INIT(&MessageOut);
    KINETIC_MESSAGE_INIT(&MessageIn);

    // Compute the size of the encoded proto buffer
    ProtoBufferLength = KineticProto_get_packed_size(&MessageOut.proto);

    // Populate the value buffer with test payload
    for (i = 0; i < Value.len; i++)
    {
        Value.data[i] = (uint8_t)(i & 0xFFu);
    }

    KineticLogger_Init(NULL);
}

void tearDown(void)
{
}

void test_KineticPDUHeader_should_have_correct_byte_packed_size(void)
{
    TEST_ASSERT_EQUAL(1+4+4, PDU_HEADER_LEN);
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN, sizeof(KineticPDUHeader));
}

void test_KineteicPDU_PDU_PROTO_MAX_LEN_should_be_1MB(void)
{
    TEST_ASSERT_EQUAL(1024*1024, PDU_PROTO_MAX_LEN);
}

void test_KineteicPDU_PDU_VALUE_MAX_LEN_should_be_1MB(void)
{
    TEST_ASSERT_EQUAL(1024*1024, PDU_VALUE_MAX_LEN);
}

void test_KineticPDU_PDU_VALUE_MAX_LEN_should_be_the_sum_of_header_protobuf_and_value_max_lengths(void)
{
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN, PDU_MAX_LEN);
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer(void)
{
    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);

    KineticPDU_Init(&PDU, &Connection, &MessageOut, BYTE_ARRAY_NONE);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Connection, PDU.connection);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&MessageOut, PDU.message);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', PDU.header.versionPrefix);

    // Validate protobuf length
    TEST_ASSERT_EQUAL_INT32(0, PDU.protobufLength);

    // Validate 'value' field is empty
    TEST_ASSERT_BYTE_ARRAY_EMPTY(PDU.value);
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer_and_value_payload(void)
{
    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);

    KineticPDU_Init(&PDU, &Connection, &MessageOut, Value);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Connection, PDU.connection);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&MessageOut, PDU.message);
    TEST_ASSERT_NULL(PDU.proto);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', PDU.header.versionPrefix);

    // Validate protobuf length
    TEST_ASSERT_EQUAL_INT32(0, PDU.protobufLength);

    // Validate value field size and content association
    TEST_ASSERT_EQUAL_BYTE_ARRAY(Value, PDU.value);
}

void test_KineticPDU_Init_should_allow_NULL_message_to_allow_inbound_PDUs_to_be_dynamically_allocated_and_associated_with_PDU(void)
{
    KineticPDU_Init(&PDU, &Connection, NULL, BYTE_ARRAY_NONE);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Connection, PDU.connection);

    // Validate KineticMessage NOT associated
    TEST_ASSERT_NULL_MESSAGE(PDU.message, "Message should not be associated with PDU, since will be dynamically allocated upon receive!");

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', PDU.header.versionPrefix);

    // Validate protobuf length
    TEST_ASSERT_EQUAL_INT32(0, PDU.protobufLength);
    TEST_ASSERT_EQUAL_NBO_INT32(0, PDU.header.protobufLength);

    // Validate value field size and content association
    TEST_ASSERT_BYTE_ARRAY_EMPTY(PDU.value);
    TEST_ASSERT_EQUAL_NBO_INT32(0, PDU.header.valueLength);
}


void test_KineticPDU_Status_should_return_status_from_protobuf_if_available(void)
{
    KineticProto proto = KINETIC_PROTO_INIT;
    KineticProto_Command cmd = KINETIC_PROTO_COMMAND_INIT;
    KineticProto_Status status = KINETIC_PROTO_STATUS_INIT;
    proto.command = &cmd;
    cmd.status = &status;
    status.code = KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH;

    KineticPDU pdu = {.proto = &proto};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH, KineticPDU_Status(&pdu));
}

void test_KineticPDU_Status_should_return_invalid_status_from_protobuf_if_protobuf_does_not_have_a_command(void)
{
    KineticProto proto = KINETIC_PROTO_INIT;
    KineticProto_Command cmd = KINETIC_PROTO_COMMAND_INIT;

    KineticPDU pdu = {.proto = &proto};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE, KineticPDU_Status(&pdu));
}

void test_KineticPDU_Status_should_return_invalid_status_from_protobuf_if_protobuf_does_not_have_a_status(void)
{
    KineticProto proto = KINETIC_PROTO_INIT;
    KineticProto_Command cmd = KINETIC_PROTO_COMMAND_INIT;
    proto.command = &cmd;

    KineticPDU pdu = {.proto = &proto};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE, KineticPDU_Status(&pdu));
}

void test_KineticPDU_Status_should_return_status_from_message_field_if_protobuf_pointer_is_NULL_and_message_contains_a_status(void)
{
    KineticMessage msg;
    KINETIC_MESSAGE_INIT(&msg);
    msg.command.status = &msg.status;
    msg.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH;
    KineticPDU pdu = {.proto = NULL, .message = &msg};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH, KineticPDU_Status(&pdu));
}

void test_KineticPDU_Status_should_return_invalid_status_if_protobuf_pointer_is_NULL_and_message_has_no_command(void)
{
    KineticMessage msg;
    KINETIC_MESSAGE_INIT(&msg);
    msg.proto.command = NULL;
    KineticPDU pdu = {.message = &msg};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE, KineticPDU_Status(&pdu));
}

void test_KineticPDU_Status_should_return_invalid_status_if_protobuf_pointer_is_NULL_and_message_has_no_status(void)
{
    KineticMessage msg;
    KINETIC_MESSAGE_INIT(&msg);
    msg.proto.command = NULL;
    KineticPDU pdu = {.message = &msg};

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE, KineticPDU_Status(&pdu));
}


void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_no_value_payload(void)
{
    bool status;

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);

    KineticPDU_Init(&PDUOut, &Connection, &MessageOut, BYTE_ARRAY_NONE);

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");
    ByteArray header = {.data = (uint8_t*)&PDUOut.header, .len = sizeof(KineticPDUHeader)};

    KineticHMAC_Init_Expect(&PDUOut.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDUOut.hmac, &PDUOut.message->proto, PDUOut.connection->key);
    KineticSocket_Write_ExpectAndReturn(456, header, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(456, &PDUOut.message->proto, true);

    status = KineticPDU_Send(&PDUOut);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_PDU_and_return_true_upon_successful_transmission_of_full_PDU_with_value_payload(void)
{
    bool status;
    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("Some arbitrary value");
    ByteArray header = {.data = (uint8_t*)&PDUOut.header, .len = sizeof(KineticPDUHeader)};

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);

    KineticPDU_Init(&PDUOut, &Connection, &MessageOut, value);

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticHMAC_Init_Expect(&PDUOut.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDUOut.hmac, &PDUOut.message->proto, PDUOut.connection->key);
    KineticSocket_Write_ExpectAndReturn(456, header, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(456, &PDUOut.message->proto, true);
    KineticSocket_Write_ExpectAndReturn(456, PDUOut.value, true);

    status = KineticPDU_Send(&PDUOut);

    TEST_ASSERT_TRUE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_header(void)
{
    bool status;
    ByteArray header = {.data = (uint8_t*)&PDUOut.header, .len = sizeof(KineticPDUHeader)};
    uint8_t valueBuffer[10];
    ByteArray value = {.data = valueBuffer, .len = sizeof(valueBuffer)};

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUOut, &Connection, &MessageOut, value);

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticHMAC_Init_Expect(&PDUOut.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDUOut.hmac, &PDUOut.message->proto, PDUOut.connection->key);
    KineticSocket_Write_ExpectAndReturn(456, header, false);

    status = KineticPDU_Send(&PDUOut);

    TEST_ASSERT_FALSE(status);
}

#if 0
void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_protobuf(void)
{
    bool status;
    uint8_t value[10];

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUOut, &Connection, &MessageOut, value, sizeof(value));

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticHMAC_Init_Expect(&PDUOut.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDUOut.hmac, &PDUOut.message->proto, PDUOut.connection->key);
    KineticSocket_Write_ExpectAndReturn(456, &PDUOut.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(456, &PDUOut.message->proto, false);

    status = KineticPDU_Send(&PDUOut);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false(void)
{
    bool status;
    uint8_t value[10];

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUOut, &Connection, &MessageOut, value, sizeof(value));

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticHMAC_Init_Expect(&PDUOut.hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDUOut.hmac, &PDUOut.message->proto, PDUOut.connection->key);
    KineticSocket_Write_ExpectAndReturn(456, &PDUOut.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(456, &PDUOut.message->proto, true);
    KineticSocket_Write_ExpectAndReturn(456, PDUOut.value, PDUOut.valueLength, false);

    status = KineticPDU_Send(&PDUOut);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_with_value_payload_for_the_exchange_and_return_true_upon_successful_transmission_of_full_PDU_upon_full_receipt_of_valid_PDU(void)
{
    bool status;
    uint8_t value[10];
    uint32_t protobufLength = 12;

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, value, sizeof(value));
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS; // Fake success for now
    MessageIn.status.has_code = true;
    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = sizeof(value);

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, true);
    KineticHMAC_Validate_ExpectAndReturn(PDUIn.proto, PDUIn.connection->key, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, value, PDUIn.valueLength, true);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_TRUE(status);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, MessageIn.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_for_the_exchange_and_return_true_upon_successful_transmission_of_full_PDU_upon_full_receipt_of_valid_PDU(void)
{
    bool status;
    uint8_t protobufLength = 12;

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, NULL, 0);
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS; // Fake success for now
    MessageIn.status.has_code = true;
    PDUIn.header.valueLength = 0;

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = 0;

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, true);
    KineticHMAC_Validate_ExpectAndReturn(PDUIn.proto, PDUIn.connection->key, true);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_TRUE(status);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, MessageIn.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_for_the_exchange_and_return_true_upon_successful_transmission_of_full_PDU_upon_full_receipt_of_valid_PDU_with_non_successful_protobuf_status(void)
{
    bool status;
    uint8_t protobufLength = 12;

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, NULL, 0);
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    MessageIn.status.has_code = true;
    PDUIn.header.valueLength = 0;

    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = 0;

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, true);
    KineticHMAC_Validate_ExpectAndReturn(PDUIn.proto, PDUIn.connection->key, true);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_TRUE(status);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR, MessageIn.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_header(void)
{
    bool status;
    uint8_t value[10];

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, value, sizeof(value));
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    MessageIn.status.has_code = true;

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), false);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_protobuf(void)
{
    bool status;
    uint8_t protobufLength = 12;
    uint8_t value[10];

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, value, sizeof(value));
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    MessageIn.status.has_code = true;

    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = 0;
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, false);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_HMAC_validation_failure(void)
{
    bool status;
    uint8_t protobufLength = 12;
    uint8_t value[10];

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, &MessageIn, value, sizeof(value));
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    MessageIn.status.has_code = true;

    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = 0;

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, true);
    KineticHMAC_Validate_ExpectAndReturn(PDUIn.proto, PDUIn.connection->key, false);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_FALSE(status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_value_field_receive_failure(void)
{
    bool status = true;
    uint8_t protobufLength = 12;
    uint8_t value[10];
    uint8_t valueLength = (uint8_t)sizeof(value);

    Connection.connected = true;
    Connection.nonBlocking = false;
    Connection.port = 1234;
    Connection.socketDescriptor = 456;
    strcpy(Connection.host, "valid-host.com");

    // KineticConnection_ConfigureHeader_Expect(&Connection, &MessageOut.header);
    KineticPDU_Init(&PDUIn, &Connection, NULL, value, valueLength);
    TEST_ASSERT_NULL(PDUIn.message);
    TEST_ASSERT_NULL(PDUIn.proto);
    TEST_ASSERT_EQUAL(0, PDUIn.protobufLength);
    TEST_ASSERT_EQUAL_PTR(value, PDUIn.value);
    TEST_ASSERT_EQUAL(sizeof(value), PDUIn.valueLength);
    PDUIn.proto = &MessageIn.proto;
    MessageIn.proto.command = &MessageIn.command;
    MessageIn.proto.command->status = &MessageIn.status;
    MessageIn.status.has_code = true;
    MessageIn.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    MessageIn.status.has_code = true;

    PDUIn.rawHeader[0] = (uint8_t)'F';

    // Fake protobuf length
    PDUIn.rawHeader[1] = 0;
    PDUIn.rawHeader[2] = 0;
    PDUIn.rawHeader[3] = 0;
    PDUIn.rawHeader[4] = protobufLength;

    // Fake value/payload length
    PDUIn.rawHeader[5] = 0;
    PDUIn.rawHeader[6] = 0;
    PDUIn.rawHeader[7] = 0;
    PDUIn.rawHeader[8] = valueLength;

    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &PDUIn.proto, PDUIn.protobufScratch, protobufLength, true);
    KineticHMAC_Validate_ExpectAndReturn(PDUIn.proto, PDUIn.connection->key, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, value, PDUIn.valueLength, false);

    status = KineticPDU_Receive(&PDUIn);

    TEST_ASSERT_FALSE(status);
    TEST_ASSERT_EQUAL(protobufLength, PDUIn.header.protobufLength);
    TEST_ASSERT_EQUAL(valueLength, PDUIn.header.valueLength);
}
#endif
