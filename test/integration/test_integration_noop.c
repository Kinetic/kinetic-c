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
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

void setUp(void)
{
    KineticClient_Init(NULL);
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    bool success;
    KineticOperation operation;
    KineticPDU request, response;
    const char* host = "localhost";
    const int port = 8899;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("123abcXYZ");
    const int socketDesc = 783;
    KineticConnection connection;
    KineticMessage requestMsg;

    ByteArray requestHeader = {
        .data = (uint8_t*)&request.header,
        .len = sizeof(KineticPDUHeader) };
    ByteArray responseHeaderRaw = {
        .data = (uint8_t*)&response.headerNBO,
        .len = sizeof(KineticPDUHeader) };
    ByteArray responseProtobuf = {
        .data = response.protobufRaw };

    // Establish connection
    KINETIC_CONNECTION_INIT(&connection, identity, key);
    connection.socketDescriptor = socketDesc; // configure dummy socket descriptor
    KineticConnection_Connect_ExpectAndReturn(&connection, host, port, false, clusterVersion, identity, key, true);
    success = KineticClient_Connect(&connection, host, port, false, clusterVersion, identity, key);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Create the operation
    operation = KineticClient_CreateOperation(&connection, &request, &response);
    TEST_ASSERT_EQUAL_PTR(&connection, operation.connection);
    TEST_ASSERT_EQUAL_PTR(&request, operation.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, operation.request->message);
    TEST_ASSERT_EQUAL_PTR(&response, operation.response);
    TEST_ASSERT_NULL(operation.response->message);
    TEST_ASSERT_NULL(operation.response->proto);

    KineticProto responseProto = KINETIC_PROTO__INIT;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND__INIT;
    response.proto = &responseProto;
    response.proto->command = &responseCommand;

    KineticProto_Header responseHeader = KINETIC_PROTO_HEADER__INIT;
    response.proto->command->header = &responseHeader;
    response.header.valueLength = 0;
    response.header.protobufLength = 123;

    KineticProto_Status responseStatus = KINETIC_PROTO_STATUS__INIT;
    response.proto->command->status = &responseStatus;
    response.proto->command->status->has_code = true;
    response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Fake the response, since kinetic_socket is mocked
    operation.response->proto = &responseProto;
    responseProto.command = &responseCommand;
    responseCommand.status = &responseStatus;
    responseStatus.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    uint8_t hmacData[64];
    responseProto.has_hmac = true;
    responseProto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &responseProto, key);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&connection);
    KineticSocket_Write_ExpectAndReturn(socketDesc, requestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(socketDesc, responseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, responseProtobuf, true);

    // Execute the operation
    KineticProto_Status_StatusCode status =
        KineticClient_NoOp(&operation);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
}
