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

void test_KineticClient_Init_should_initialize_the_logger(void)
{
    KineticLogger_Init_Expect("some/file.log");

    KineticClient_Init("some/file.log");
}

void test_KineticClient_Connect_should_configure_a_connection(void)
{
    KineticConnection connection;

    connection.connected = false; // Ensure gets set appropriately by internal connect call

    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "somehost.com", 321, true, true);

    TEST_ASSERT_TRUE(KineticClient_Connect(&connection, "somehost.com", 321, true));

    TEST_ASSERT_TRUE(connection.connected);
}

void test_KineticClient_Connect_should_log_a_failed_connection_and_return_false(void)
{
    KineticConnection connection;

    // Ensure appropriately updated per internal connect call result
    connection.connected = true;
    connection.socketDescriptor = 333;

    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "somehost.com", 123, true, false);
    KineticLogger_Log_Expect("Failed creating connection to somehost.com:123");

    TEST_ASSERT_FALSE(KineticClient_Connect(&connection, "somehost.com", 123, true));

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_EQUAL(-1, connection.socketDescriptor);
}

void test_KineticAPI_ConfigureExchange_should_configure_specified_KineticExchange(void)
{
    bool success;
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    KineticExchange exchange;
    const char* key = "U_i472";

    KineticExchange_Init_Expect(&exchange, 1234, key, strlen(key), &connection);
    KineticExchange_SetClusterVersion_Expect(&exchange, 76543210);
    KineticExchange_ConfigureConnectionID_Expect(&exchange);

    success = KineticClient_ConfigureExchange(&exchange, &connection, 76543210, 1234, key, strlen(key));

    TEST_ASSERT_TRUE(success);
}

void test_KineticClient_CreateOperation_should_create_configure_and_return_a_valid_operation_instance(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;
    KineticMessage requestMsg;

    KineticMessage_Init_Expect(&requestMsg);
    KineticPDU_Init_Expect(&request, &exchange, &requestMsg, NULL, 0);
    KineticPDU_Init_Expect(&response, &exchange, NULL, NULL, 0);

    op = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&requestMsg, op.request->message);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
    TEST_ASSERT_NULL(op.response->message);
    TEST_ASSERT_NULL(op.response->proto);
}
