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
#include <protobuf-c/protobuf-c.h>
#include "kinetic_proto.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_exchange.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_logger.h"
#include "mock_kinetic_operation.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticApi_Init_should_initialize_the_logger(void)
{
    KineticLogger_Init_Expect("some/file.log");

    KineticApi_Init("some/file.log");
}

void test_KineticApi_Connect_should_configure_a_connection(void)
{
    KineticConnection connection;

    connection.connected = false; // Ensure gets set appropriately by internal connect call

    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "somehost.com", 321, true, true);

    TEST_ASSERT_TRUE(KineticApi_Connect(&connection, "somehost.com", 321, true));

    TEST_ASSERT_TRUE(connection.connected);
}

void test_KineticApi_Connect_should_log_a_failed_connection_and_return_false(void)
{
    KineticConnection connection;

    // Ensure appropriately updated per internal connect call result
    connection.connected = true;
    connection.socketDescriptor = 333;

    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "somehost.com", 123, true, false);
    KineticLogger_Log_Expect("Failed creating connection to somehost.com:123");

    TEST_ASSERT_FALSE(KineticApi_Connect(&connection, "somehost.com", 123, true));

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_EQUAL(-1, connection.socketDescriptor);
}

void test_KineticAPI_ConfigureExchange_should_configure_specified_KineticExchange(void)
{
    bool success;
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    KineticExchange exchange;
    uint8_t key[8];

    KineticExchange_Init_Expect(&exchange, 1234, key, sizeof(key), &connection);
    KineticExchange_ConfigureConnectionID_Expect(&exchange);

    success = KineticApi_ConfigureExchange(&exchange, &connection, 1234, key, sizeof(key));

    TEST_ASSERT_TRUE(success);
}

void test_KineticApi_CreateOperation_should_create_configure_and_return_a_valid_operation_instance(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;
    KineticMessage requestMsg, responseMsg;

    KineticMessage_Init_Expect(&requestMsg);
    KineticPDU_Init_Expect(&request, &exchange, &requestMsg, NULL, 0);
    KineticMessage_Init_Expect(&responseMsg);
    KineticPDU_Init_Expect(&response, &exchange, &responseMsg, NULL, 0);

    op = KineticApi_CreateOperation(&exchange, &request, &requestMsg, &response, &responseMsg);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, op.request->protobuf);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
    TEST_ASSERT_EQUAL_PTR(&responseMsg, op.response->protobuf);
}

void test_KineticApi_NoOp_should_send_NOOP_command(void)
{
    KineticConnection connection;
    KineticExchange exchange;
    KineticOperation operation;
    KineticMessage requestMsg, responseMsg;
    KineticPDU request, response;
    KineticProto_Status_StatusCode status;
    int64_t identity = 1234;
    int64_t connectionID = 5678;

    exchange.connection = &connection;

    request.exchange = &exchange;
    KINETIC_MESSAGE_INIT(&requestMsg);
    request.protobuf = &requestMsg;

    response.exchange = &exchange;
    KINETIC_MESSAGE_INIT(&responseMsg);
    response.protobuf = &responseMsg;
    response.protobuf->status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    KINETIC_OPERATION_INIT(&operation, &exchange, &request, &response);

    KineticExchange_IncrementSequence_Expect(&exchange);
    KineticOperation_BuildNoop_Expect(&operation);
    KineticPDU_Send_ExpectAndReturn(&request, true);
    KineticPDU_Receive_ExpectAndReturn(&response, true);

    status = KineticApi_NoOp(&operation);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&exchange, response.exchange);
}
