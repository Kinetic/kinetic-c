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

#include "kinetic_client.h"
#include "unity.h"
#include "unity_helper.h"
#include <stdio.h>
#include "protobuf-c/protobuf-c.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"

void setUp(void)
{
    KineticClient_Init(NULL);
}

void tearDown(void)
{
}

void test_Get_should_retrieve_an_object_from_the_device_and_store_in_supplied_byte_array(void)
{
    KineticOperation operation;
    KineticPDU request, response;
    const char* host = "localhost";
    const int port = 8899;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const ByteArray hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("123abcXYZ");
    const int socketDesc = 783;
    KineticConnection connection;
    KineticMessage requestMsg;

    // Establish connection
    KINETIC_CONNECTION_INIT(&connection, identity, hmacKey);
    connection.socketDescriptor = socketDesc; // configure dummy socket descriptor
    KineticConnection_Connect_ExpectAndReturn(&connection, host, port, false, clusterVersion, identity, hmacKey, true);
    bool success = KineticClient_Connect(&connection, host, port, false, clusterVersion, identity, hmacKey);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Create the operation
    operation = KineticClient_CreateOperation(&connection, &request, &response);

    KineticProto responseProto = KINETIC_PROTO__INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND__INIT;
    response.proto = &responseProto;
    response.proto->command = &responseCommand;

    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER__INIT;
    response.proto->command->header = &responseHeader;

    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS__INIT;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Create operation-specific data and paylod
    ByteArray valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    const Kinetic_KeyValue metadata = {.key = valueKey};

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    uint8_t hmacData[64];
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &responseProto, hmacKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&connection);
    ByteArray requestHeader = {.data = (uint8_t*)&request.header, .len = sizeof(KineticPDUHeader)};
    KineticSocket_Write_ExpectAndReturn(socketDesc, requestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);

    // Fake value/payload length
    const char* valueData = "lorem ipsum...";
    ByteArray value = {.data = (uint8_t*)valueData, .len = strlen(valueData)};
    response.header = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
        .protobufLength = response.header.protobufLength,
        .valueLength = value.len,
    };
    response.headerNBO = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
        .protobufLength = KineticNBO_FromHostU32(response.header.protobufLength),
        .valueLength = KineticNBO_FromHostU32(value.len),
    };
    ByteArray responseProtobuf = {
        .data = response.protobufRaw,
        .len = response.header.protobufLength
    };
    KineticPDU_AttachValuePayload(&response, value);

    // Receive the response
    ByteArray responseHeaderRaw = {.data = (uint8_t*)&response.headerNBO, .len = sizeof(KineticPDUHeader)};
    KineticSocket_Read_ExpectAndReturn(socketDesc, responseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, responseProtobuf, true);
    ByteArray expectedValueArray = value;
    KineticSocket_Read_ExpectAndReturn(socketDesc, value, true);

    LOG_LOCATION;
    KineticLogger_LogByteArray("expectedValueArray", expectedValueArray);
    KineticLogger_LogByteArray("value", value);

    // Execute the operation
    KineticProto_Status_StatusCode status =
        KineticClient_Get(&operation, &metadata, value);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(value.data, response.value.data);
}

void test_Get_should_create_retrieve_an_object_from_the_device_and_store_in_embedded_ByteArray(void)
{
    KineticOperation operation;
    KineticPDU request, response;
    const char* host = "localhost";
    const int port = 8899;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const ByteArray hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("123abcXYZ");
    const int socketDesc = 783;
    KineticConnection connection;
    KineticMessage requestMsg;

    // Establish connection
    KINETIC_CONNECTION_INIT(&connection, identity, hmacKey);
    connection.socketDescriptor = socketDesc; // configure dummy socket descriptor
    KineticConnection_Connect_ExpectAndReturn(&connection, host, port, false, clusterVersion, identity, hmacKey, true);
    bool success = KineticClient_Connect(&connection, host, port, false, clusterVersion, identity, hmacKey);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Create the operation
    operation = KineticClient_CreateOperation(&connection, &request, &response);

    KineticProto responseProto = KINETIC_PROTO__INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND__INIT;
    response.proto = &responseProto;
    response.proto->command = &responseCommand;

    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER__INIT;
    response.proto->command->header = &responseHeader;

    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS__INIT;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Create operation-specific data and paylod
    ByteArray valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    const Kinetic_KeyValue metadata = {.key = valueKey};

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    uint8_t hmacData[64];
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &responseProto, hmacKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&connection);
    ByteArray requestHeader = {.data = (uint8_t*)&request.header, .len = sizeof(KineticPDUHeader)};
    KineticSocket_Write_ExpectAndReturn(socketDesc, requestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);

    // Fake value/payload length
    const char* valueData = "hoop de do... blah blah blah";
    ByteArray value = {.data = (uint8_t*)valueData, .len = strlen(valueData)};
    response.header = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
        .protobufLength = response.header.protobufLength,
        .valueLength = value.len,
    };
    response.headerNBO = (KineticPDUHeader) {
        .versionPrefix = (uint8_t)'F',
        .protobufLength = KineticNBO_FromHostU32(response.header.protobufLength),
        .valueLength = KineticNBO_FromHostU32(value.len),
    };
    ByteArray responseProtobuf = {
        .data = response.protobufRaw,
        .len = response.header.protobufLength
    };

    // Copy the dummy value data into the response buffer
    KineticPDU_EnableValueBufferWithLength(&response, value.len);
    memcpy(&response, valueData, strlen(valueData));

    // Receive the response
    ByteArray responseHeaderRaw = {.data = (uint8_t*)&response.headerNBO, .len = sizeof(KineticPDUHeader)};
    KineticSocket_Read_ExpectAndReturn(socketDesc, responseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, responseProtobuf, true);
    ByteArray expectedValueArray = value;
    KineticSocket_Read_ExpectAndReturn(socketDesc, value, true);

    LOG_LOCATION;
    KineticLogger_LogByteArray("expectedValueArray", expectedValueArray);
    KineticLogger_LogByteArray("value", value);

    // Execute the operation
    KineticProto_Status_StatusCode status =
        KineticClient_Get(&operation, &metadata, value);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(value.data, response.value.data);    
}
