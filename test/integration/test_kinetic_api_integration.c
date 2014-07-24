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
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    TEST_IGNORE_MESSAGE("Need to implement!");
}

#if 0
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
    KineticMessage_Init_Expect(&requestMsg);
    KineticPDU_Init_Expect(&request, &exchange, &requestMsg, NULL, 0);
    KineticOperation_BuildNoop_Expect(&operation);
    KineticPDU_Send_ExpectAndReturn(&request, true);
    KineticPDU_Init_Expect(&response, &exchange, &responseMsg, NULL, 0);
    KineticPDU_Receive_ExpectAndReturn(&response, true);

    status = KineticApi_NoOp(&operation);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&exchange, response.exchange);
}
#endif
