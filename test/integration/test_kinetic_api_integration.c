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

#include "kinetic_api.h"
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
    KineticApi_Init(NULL);
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    bool success;
    KineticProto_Status_StatusCode statusCode;

    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const char* key = "123abcXYZ";
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

    // Carry out the NOOP operation...

    // Establish connection
    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "localhost", 8999, true, true);
    success = KineticApi_Connect(&connection, "localhost", 8999, true);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    // Configure the exchange
    success = KineticApi_ConfigureExchange(&exchange, &connection,
        clusterVersion, identity, key, strlen(key));
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed configuring exchange!");

    // Create the NOOP operation
    operation = KineticApi_CreateOperation(&exchange, &request, &requestMsg, &response);
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

    // Execute the request and receive the response
    KineticSocket_Write_ExpectAndReturn(socketDesc, &request.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg.proto, true);
    KineticSocket_Read_ExpectAndReturn(socketDesc, &response.rawHeader, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &response.proto, response.protobufScratch, 0, true);
    statusCode = KineticApi_NoOp(&operation);
    // TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, statusCode);
}
