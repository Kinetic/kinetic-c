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

void test_KineticApi_Connect_should_create_a_connection(void)
{
    KineticConnection dummy;
    KineticConnection result;

    dummy.Connected = false; // Ensure gets set appropriately by internal connect call

    KineticConnection_Init_ExpectAndReturn(dummy);
    KineticConnection_Connect_ExpectAndReturn(&dummy, "somehost.com", 321, true, true);

    result = KineticApi_Connect("somehost.com", 321, true);

    TEST_ASSERT_TRUE(result.Connected);
}

void test_KineticApi_Connect_should_log_a_failed_connection(void)
{
    KineticConnection dummy;
    KineticConnection result;

    // Ensure appropriately updated per internal connect call result
    dummy.Connected = true;
    dummy.FileDescriptor = 333;

    KineticConnection_Init_ExpectAndReturn(dummy);
    KineticConnection_Connect_ExpectAndReturn(&dummy, "somehost.com", 123, true, false);
    KineticLogger_Log_Expect("Failed creating connection to somehost.com:123");

    result = KineticApi_Connect("somehost.com", 123, true);

    TEST_ASSERT_FALSE(result.Connected);
    TEST_ASSERT_EQUAL(-1, result.FileDescriptor);
}

void test_KineticApi_SendNoop_should_send_NOOP_command(void)
{
    KineticConnection connection;
    KineticExchange exchange;
    KineticMessage requestMsg, responseMsg;
    KineticPDU request, response;
    KineticProto_Status_StatusCode status;
    int64_t identity = 1234;
    int64_t connectionID = 5678;
    uint8_t requestBuffer[3*(1024*1024)];
    uint8_t responseBuffer[3*(1024*1024)];

    exchange.connection = &connection;

    request.exchange = &exchange;
    request.message = &requestMsg;

    response.exchange = &exchange;
    response.message = &responseMsg;

    KineticConnection_Init_ExpectAndReturn(connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "salgood.com", 88, false, true);

    connection = KineticApi_Connect("salgood.com", 88, false);

    KineticExchange_Init_Expect(&exchange, identity, connectionID, &connection);
    KineticMessage_Init_Expect(&requestMsg);
    KineticMessage_BuildNoop_Expect(&requestMsg);
    KineticPDU_Init_Expect(&request, &exchange, requestBuffer, &requestMsg, NULL, 0);
    KineticConnection_SendPDU_ExpectAndReturn(&request, true);
    KineticPDU_Init_Expect(&response, &exchange, responseBuffer, &responseMsg, NULL, 0);
    KineticConnection_ReceivePDU_ExpectAndReturn(&response, true);

    status = KineticApi_SendNoop(&request, &response, requestBuffer, responseBuffer);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&exchange, response.exchange);
}
