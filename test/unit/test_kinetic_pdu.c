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
#include <stdlib.h>

static KineticPDU PDU;
static KineticConnection Connection;
static KineticSession Session;
static ByteArray Key;
static uint8_t ValueBuffer[KINETIC_OBJ_SIZE];
static ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void EnableAndSetPDUConnectionID(KineticPDU* pdu, int64_t connectionID)
{
    assert(pdu != NULL);
    pdu->proto = &PDU.protoData.message.message;
    pdu->protoData.message.has_command = true;
    pdu->protoData.message.command.header = &PDU.protoData.message.header;
    pdu->protoData.message.command.header->connectionID = connectionID;
}

void EnableAndSetPDUStatus(KineticPDU* pdu, KineticProto_Command_Status_StatusCode status)
{
    assert(pdu != NULL);
    pdu->proto = &PDU.protoData.message.message;
    pdu->protoData.message.has_command = true;
    pdu->protoData.message.command.status = &PDU.protoData.message.status;
    pdu->protoData.message.command.status->code = status;
    pdu->protoData.message.command.status->has_code = true;
}


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
    KineticLogger_Init("stdout");
}

void tearDown()
{
    KineticLogger_Close();
}

#if 0
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

void test_KineteicPDU_KINETIC_OBJ_SIZE_should_be_1MB(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(1024 * 1024, KINETIC_OBJ_SIZE);
}

void test_KineticPDU_KINETIC_OBJ_SIZE_should_be_the_sum_of_header_protobuf_and_value_max_lengths(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + KINETIC_OBJ_SIZE, PDU_MAX_LEN);
}


void test_KineticPDU_Init_should_populate_the_PDU_and_buffer_with_the_supplied_buffer(void)
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

    TEST_ASSERT_TRUE(PDU.protoData.message.message.has_authType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH, PDU.protoData.message.message.authType);
    TEST_ASSERT_TRUE(PDU.protoData.message.hmacAuth.has_identity);
    TEST_ASSERT_EQUAL_INT64(37, PDU.protoData.message.hmacAuth.identity);

    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788,
                            PDU.protoData.message.header.clusterVersion);
    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_connectionID);
    TEST_ASSERT_EQUAL_INT64(8765432, PDU.protoData.message.header.connectionID);
    TEST_ASSERT_TRUE(PDU.protoData.message.header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(24, PDU.protoData.message.header.sequence);
}


void test_KineticPDU_AttachEntry_should_attach_specified_KineticEntry(void)
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
    TEST_ASSERT_EQUAL(3, PDU.entry.value.bytesUsed);
    TEST_ASSERT_EQUAL(3, entry.value.bytesUsed);
    TEST_ASSERT_EQUAL(sizeof(data), entry.value.array.len);
}


void test_KINETIC_PDU_INIT_WITH_COMMAND_should_initialize_PDU_and_protobuf_message(void)
{
    LOG_LOCATION;
    memset(&PDU, 0, sizeof(KineticPDU));
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);

    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SERVICE_BUSY;
    PDU.protoData.message.command.status = &PDU.protoData.message.status;

    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SERVICE_BUSY,
        PDU.protoData.message.command.status->code);
}
#endif


void test_KineticPDU_Send_should_transmit_PDU_with_no_value_payload(void)
{
    LOG_LOCATION;
    KineticProto_Message* msg = &PDU.protoData.message.message;

    // KineticProto_Message__init(msg);
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    KineticEntry entry = {.value = BYTE_BUFFER_NONE};
    KineticPDU_AttachEntry(&PDU, &entry);

    uint8_t packedCommandBytes[1024];

    // Pack message `command` element in order to precalculate fully packed message size 
    size_t expectedCommandLen = KineticProto_command__get_packed_size(&PDU.protoData.message.command);
    PDU.protoData.message.message.commandBytes.data = packedCommandBytes;
    assert(PDU.protoData.message.message.commandBytes.data != NULL);
    size_t packedCommandLen = KineticProto_command__pack(
        &PDU.protoData.message.command,
        PDU.protoData.message.message.commandBytes.data);
    assert(packedCommandLen == expectedCommandLen);
    PDU.protoData.message.message.commandBytes.len = packedCommandLen;
    PDU.protoData.message.message.has_commandBytes = true;

    // Create NBO copy of header for sending
    PDU.header.versionPrefix = 'F';
    PDU.header.protobufLength = KineticProto_Message__get_packed_size(msg);
    PDU.header.valueLength = 0;
    PDU.headerNBO.versionPrefix = PDU.header.versionPrefix;
    PDU.headerNBO.protobufLength = KineticNBO_FromHostU32(PDU.header.protobufLength);
    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(PDU.header.valueLength);

    // Setup expectations for interaction
    KineticHMAC_Init_Expect(&PDU.hmac,
        KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
        &PDU.protoData.message.message, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(
        Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(
        Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticPDU_Send(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticPDU_Send_should_send_PDU_with_value_payload(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    uint8_t valueData[128];
    ByteBuffer valueBuffer = ByteBuffer_Create(valueData, sizeof(valueData), 0);
    ByteBuffer_AppendCString(&valueBuffer, "Some arbitrary value");
    KineticEntry entry = {.value = valueBuffer};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
        &PDU.protoData.message.message, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &PDU.entry.value, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticPDU_Send(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_header(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData), strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.message, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SOCKET_ERROR);

    KineticStatus status = KineticPDU_Send(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_ERROR, status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_false_upon_failure_to_send_protobuf(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    char valueData[] = "Some arbitrary value";
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, strlen(valueData), strlen(valueData))};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac,
        &PDU.protoData.message.message, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SOCKET_TIMEOUT);

    KineticStatus status = KineticPDU_Send(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_TIMEOUT, status);
}

void test_KineticPDU_Send_should_send_the_specified_message_and_return_KineticStatus_if_value_write_fails(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    uint8_t valueData[128];
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, sizeof(valueData), 0)};
    ByteBuffer_AppendCString(&entry.value, "Some arbitrary value");
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticHMAC_Init_Expect(&PDU.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&PDU.hmac, &PDU.protoData.message.message, PDU.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &entry.value, KINETIC_STATUS_SOCKET_TIMEOUT);

    KineticStatus status = KineticPDU_Send(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_TIMEOUT, status);
}


void test_KineticPDU_Receive_should_receive_a_message_with_value_payload_and_return_true_upon_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    // Fake value/payload length
    uint8_t data[1024];
    ByteArray expectedValue = ByteArray_Create(data, sizeof(data));
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, expectedValue.len, KINETIC_STATUS_SUCCESS);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    EnableAndSetPDUConnectionID(&PDU, 12345);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS,
                      PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_return_true_upon_successful_receipt_of_valid_PDU(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    EnableAndSetPDUConnectionID(&PDU, 12345);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS,
        PDU.protoData.message.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_value_payload_and_forward_status_upon_receipt_of_valid_PDU_with_non_successful_status(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    // Fake value/payload length
    uint8_t data[1024];
    ByteArray expectedValue = {.data = data, .len = sizeof(data)};
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR, status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR,
        PDU.protoData.message.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_header(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_CONNECTION_ERROR);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_failure_to_read_protobuf(void)
{
    LOG_LOCATION;

    KineticPDU_Init(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    PDU.protoData.message.status.has_code = true;

    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);
    PDU.headerNBO = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
         .protobufLength = KineticNBO_FromHostU32(12),
          .valueLength = 0
    };

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_DEVICE_BUSY);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_BUSY, status)
}

void test_KineticPDU_Receive_should_receive_a_message_with_no_authentication_type(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS);
    PDU.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS;
    PDU.proto->hmacAuth = NULL;
    PDU.proto->pinAuth = NULL;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);
    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(0);

    // Pack message `command` element in order to precalculate fully packed message size 
    uint8_t packedCommandBytes[1024];
    size_t expectedCommandLen = KineticProto_command__get_packed_size(&PDU.protoData.message.command);
    PDU.protoData.message.message.commandBytes.data = packedCommandBytes;
    assert(PDU.protoData.message.message.commandBytes.data != NULL);
    size_t packedCommandLen = KineticProto_command__pack(
        &PDU.protoData.message.command,
        PDU.protoData.message.message.commandBytes.data);
    assert(packedCommandLen == expectedCommandLen);
    PDU.protoData.message.message.commandBytes.len = packedCommandLen;
    PDU.protoData.message.message.has_commandBytes = true;

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS,
        PDU.protoData.message.status.code);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_HMAC_validation_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, false);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR, status);
}

void test_KineticPDU_Receive_should_receive_a_message_for_the_exchange_and_return_false_upon_value_field_receive_failure(void)
{
    LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);
    PDU.headerNBO = (KineticPDUHeader) {
        .versionPrefix = 'F', .protobufLength = KineticNBO_ToHostU32(17),
         .valueLength = KineticNBO_ToHostU32(124)
    };

    // Fake value/payload length
    uint8_t data[124];
    size_t bytesToRead = sizeof(data);
    ByteArray expectedValue = ByteArray_Create(data, sizeof(data));
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, bytesToRead, KINETIC_STATUS_SOCKET_ERROR);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_ERROR, status);
    TEST_ASSERT_EQUAL(17, PDU.header.protobufLength);
    TEST_ASSERT_EQUAL(bytesToRead, expectedValue.len);
    TEST_ASSERT_EQUAL(expectedValue.len, PDU.header.valueLength);
}

void test_KineticPDU_Receive_should_receive_a_message_and_update_the_ConnectionID_for_the_connection_when_provided(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    // Fake value/payload length
    uint8_t data[124];
    size_t bytesToRead = sizeof(data);
    ByteArray expectedValue = {.data = data, .len = sizeof(data)};
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, bytesToRead, KINETIC_STATUS_SUCCESS);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    EnableAndSetPDUConnectionID(&PDU, 12345);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS, PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL(12345, PDU.protoData.message.command.header->connectionID);
    TEST_ASSERT_EQUAL(12345, PDU.connection->connectionID);
}

void test_KineticPDU_Receive_should_receive_a_message_and_NOT_update_the_ConnectionID_for_the_connection_if_not_provided(void)
{
    LOG_LOCATION;
    Connection.connectionID = 98765;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    PDU.protoData.message.status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS;
    PDU.protoData.message.status.has_code = true;
    ByteBuffer headerNBO = ByteBuffer_Create(&PDU.headerNBO, sizeof(KineticPDUHeader), 0);

    // Fake value/payload length
    uint8_t data[124];
    size_t bytesToRead = sizeof(data);
    ByteArray expectedValue = {.data = data, .len = sizeof(data)};
    ByteArray_FillWithDummyData(expectedValue);
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(expectedValue)};
    KineticPDU_AttachEntry(&PDU, &entry);

    KineticSocket_Read_ExpectAndReturn(Connection.socket, &headerNBO, sizeof(KineticPDUHeader), KINETIC_STATUS_SUCCESS);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &PDU, KINETIC_STATUS_SUCCESS);
    KineticHMAC_Validate_ExpectAndReturn(PDU.proto, PDU.connection->session.hmacKey, true);
    KineticSocket_Read_ExpectAndReturn(Connection.socket, &entry.value, bytesToRead, KINETIC_STATUS_SUCCESS);

    PDU.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    EnableAndSetPDUStatus(&PDU, KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS);

    KineticStatus status = KineticPDU_Receive(&PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS, PDU.protoData.message.status.code);
    TEST_ASSERT_EQUAL(98765, PDU.protoData.message.command.header->connectionID);
    TEST_ASSERT_EQUAL(98765, PDU.connection->connectionID);
}



void test_KineticPDU_GetStatus_should_return_KINETIC_STATUS_INVALID_if_no_KineticProto_Command_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    status = KineticPDU_GetStatus(NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto = NULL;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.proto = &PDU.protoData.message.message;
    PDU.protoData.message.has_command = false;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.protoData.message.has_command = true;
    PDU.protoData.message.command.status = NULL;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.protoData.message.command.status = &PDU.protoData.message.status;
    PDU.protoData.message.command.status->has_code = false;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    PDU.protoData.message.command.status->has_code = true;
    PDU.protoData.message.command.status->code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
}

void test_KineticPDU_GetStatus_should_return_appropriate_KineticStatus_based_on_KineticProto_Command_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    PDU.proto = &PDU.protoData.message.message;
    PDU.protoData.message.has_command = true;
    PDU.command = &PDU.protoData.message.command;
    PDU.protoData.message.command.status = &PDU.protoData.message.status;
    PDU.protoData.message.command.status->has_code = true;

    PDU.protoData.message.command.status->code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    PDU.protoData.message.command.status->code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR;
    status = KineticPDU_GetStatus(&PDU);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticPDU_GetKeyValue_should_return_NULL_message_has_no_KeyValue(void)
{
    LOG_LOCATION;

    KineticProto_Command_KeyValue* keyValue;

    PDU.proto = NULL;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.proto = &PDU.protoData.message.message;
    PDU.protoData.message.has_command = false;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.protoData.message.has_command = true;
    PDU.command = NULL;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

    PDU.protoData.message.has_command = true;
    PDU.command = &PDU.protoData.message.command;
    keyValue = KineticPDU_GetKeyValue(&PDU);
    TEST_ASSERT_NULL(keyValue);

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


void test_KineticPDU_GetKeyRange_should_return_the_KineticProto_Command_Range_from_the_message_if_avaliable(void)
{ LOG_LOCATION;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &Connection);
    KineticProto_Command_Range* range;

    PDU.command->body = NULL;

    range = KineticPDU_GetKeyRange(&PDU);
    TEST_ASSERT_NULL(range);

    PDU.command->body = &PDU.protoData.message.body;
    PDU.command->body->range = NULL;
    range = KineticPDU_GetKeyRange(&PDU);
    TEST_ASSERT_NULL(range);

    PDU.command->body->range = &PDU.protoData.message.keyRange;
    range = KineticPDU_GetKeyRange(&PDU);
    TEST_ASSERT_EQUAL_PTR(&PDU.protoData.message.keyRange, range);
}
