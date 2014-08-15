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
#include "protobuf-c.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"

void setUp(void)
{
    KineticClient_Init(NULL);
}

void tearDown(void)
{
}

void test_Put_should_create_new_object_on_device(void)
{
    bool success;
    KineticProto_Status_StatusCode statusCode;

    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const int socketDesc = 783;
    KineticConnection connection = {
        .socketDescriptor = socketDesc // Fill in, since KineticConnection is mocked
    };
    KineticMessage requestMsg;
    uint8_t hmacData[64];
    KineticHMAC respTempHMAC;
    KineticProto responseProto = KINETIC_PROTO_INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND_INIT;
    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER_INIT;
    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS_INIT;
    char* newVersion = "v1.0";
    char* key = "my_key_3.1415927";
    char* tag = "SomeTagValue";
    uint8_t value[1024*1024];
    int64_t valueLength = (int64_t)sizeof(value);
    int i;
    for (i = 0; i < valueLength; i++)
    {
        value[i] = (uint8_t)(0x0ff & i);
    }

    response.header.valueLength = 0;
    response.header.protobufLength = 123;
    response.proto = &responseProto;
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    response.proto->command = &responseCommand;
    response.proto->command->header = &responseHeader;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    KineticHMAC_Populate(&respTempHMAC, &responseProto, key, strlen(key));

    // Carry out the PUT operation...

    // Establish connection
    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "localhost", 8999, true, true);
    success = KineticClient_Connect(&connection, "localhost", 8999, true);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Configure the exchange
    success = KineticClient_ConfigureExchange(&exchange, &connection,
        clusterVersion, identity, key, strlen(key));
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed configuring exchange!");

    // Create the PUT operation
    operation = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);
    TEST_ASSERT_EQUAL_PTR(&exchange, operation.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, operation.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, operation.request->message);
    TEST_ASSERT_EQUAL_PTR(&response, operation.response);
    TEST_ASSERT_NULL(operation.response->message);
    TEST_ASSERT_NULL(operation.response->proto);

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Send the request
    KineticSocket_Write_ExpectAndReturn(socketDesc, &request.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);
    KineticSocket_Write_ExpectAndReturn(socketDesc, value, valueLength, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(socketDesc, &response.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, response.protobufScratch, 0, true);

    // Execute the Put operation
    statusCode = KineticClient_Put(&operation, newVersion, key, NULL, tag, value, valueLength);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, statusCode);
}

void test_Put_should_update_object_data_on_device(void)
{
    bool success;
    KineticProto_Status_StatusCode statusCode;

    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const int socketDesc = 783;
    KineticConnection connection = {
        .socketDescriptor = socketDesc // Fill in, since KineticConnection is mocked
    };
    KineticMessage requestMsg;
    uint8_t hmacData[64];
    KineticHMAC respTempHMAC;
    KineticProto responseProto = KINETIC_PROTO_INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND_INIT;
    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER_INIT;
    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS_INIT;
    char* key = "my_key_3.1415927";
    char* dbVersion = "v1.0";
    char* tag = "SomeTagValue";
    uint8_t value[1024*1024];
    int64_t valueLength = (int64_t)sizeof(value);
    int i;
    for (i = 0; i < valueLength; i++)
    {
        value[i] = (uint8_t)(0x0ff & i);
    }

    response.header.valueLength = 0;
    response.header.protobufLength = 123;
    response.proto = &responseProto;
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    response.proto->command = &responseCommand;
    response.proto->command->header = &responseHeader;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    KineticHMAC_Populate(&respTempHMAC, &responseProto, key, strlen(key));

    // Carry out the PUT operation...

    // Establish connection
    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "localhost", 8999, true, true);
    success = KineticClient_Connect(&connection, "localhost", 8999, true);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Configure the exchange
    success = KineticClient_ConfigureExchange(&exchange, &connection,
        clusterVersion, identity, key, strlen(key));
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed configuring exchange!");

    // Create the PUT operation
    operation = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);
    TEST_ASSERT_EQUAL_PTR(&exchange, operation.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, operation.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, operation.request->message);
    TEST_ASSERT_EQUAL_PTR(&response, operation.response);
    TEST_ASSERT_NULL(operation.response->message);
    TEST_ASSERT_NULL(operation.response->proto);

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Send the request
    KineticSocket_Write_ExpectAndReturn(socketDesc, &request.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);
    KineticSocket_Write_ExpectAndReturn(socketDesc, value, valueLength, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(socketDesc, &response.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, response.protobufScratch, 0, true);

    // Execute the Put operation
    statusCode = KineticClient_Put(&operation, NULL, key, dbVersion, tag, value, valueLength);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, statusCode);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{
    bool success;
    KineticProto_Status_StatusCode statusCode;

    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const int socketDesc = 783;
    KineticConnection connection = {
        .socketDescriptor = socketDesc // Fill in, since KineticConnection is mocked
    };
    KineticMessage requestMsg;
    uint8_t hmacData[64];
    KineticHMAC respTempHMAC;
    KineticProto responseProto = KINETIC_PROTO_INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND_INIT;
    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER_INIT;
    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS_INIT;
    char* key = "my_key_3.1415927";
    char* dbVersion = "v1.0";
    char* newVersion = "v2.0";
    char* tag = "SomeTagValue";
    uint8_t value[1024*1024];
    int64_t valueLength = (int64_t)sizeof(value);
    int i;
    for (i = 0; i < valueLength; i++)
    {
        value[i] = (uint8_t)(0x0ff & i);
    }

    response.header.valueLength = 0;
    response.header.protobufLength = 123;
    response.proto = &responseProto;
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    response.proto->command = &responseCommand;
    response.proto->command->header = &responseHeader;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    KineticHMAC_Populate(&respTempHMAC, &responseProto, key, strlen(key));

    // Carry out the PUT operation...

    // Establish connection
    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "localhost", 8999, true, true);
    success = KineticClient_Connect(&connection, "localhost", 8999, true);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Configure the exchange
    success = KineticClient_ConfigureExchange(&exchange, &connection,
        clusterVersion, identity, key, strlen(key));
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed configuring exchange!");

    // Create the PUT operation
    operation = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);
    TEST_ASSERT_EQUAL_PTR(&exchange, operation.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, operation.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, operation.request->message);
    TEST_ASSERT_EQUAL_PTR(&response, operation.response);
    TEST_ASSERT_NULL(operation.response->message);
    TEST_ASSERT_NULL(operation.response->proto);

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Send the request
    KineticSocket_Write_ExpectAndReturn(socketDesc, &request.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);
    KineticSocket_Write_ExpectAndReturn(socketDesc, value, valueLength, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(socketDesc, &response.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, response.protobufScratch, 0, true);

    // Execute the Put operation
    statusCode = KineticClient_Put(&operation, newVersion, key, dbVersion, tag, value, valueLength);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, statusCode);
}
