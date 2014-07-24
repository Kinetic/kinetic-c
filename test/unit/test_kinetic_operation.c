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
#include <protobuf-c/protobuf-c.h>
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "mock_kinetic_exchange.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KINETIC_OPERATION_INIT_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;

    KINETIC_OPERATION_INIT(&op, &exchange, &request, &response);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
}

void test_KineticOperation_Init_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;

    KineticOperation_Init(&op, &exchange, &request, &response);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
}

void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    KineticExchange exchange;
    KineticMessage requestMsg, responseMsg;
    KineticPDU request, response;
    KineticOperation op;
    KINETIC_MESSAGE_INIT(&requestMsg);
    KINETIC_MESSAGE_INIT(&responseMsg);
    KINETIC_OPERATION_INIT(&op, &exchange, &request, &response);
    request.protobuf = &requestMsg;
    response.protobuf = &responseMsg;

    KineticMessage_Init_Expect(&requestMsg);
    KineticMessage_Init_Expect(&responseMsg);
    KineticExchange_ConfigureHeader_Expect(&exchange, &requestMsg.header);

    KineticOperation_BuildNoop(&op);

    // NOOP
    // The NOOP operation can be used as a quick test of whether the Kinetic
    // Device is running and available. If the Kinetic Device is running,
    // this operation will always succeed.
    //
    // Request Message:
    //
    // command {
    //   header {
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //     messageType: NOOP
    //   }
    // }
    // hmac: "..."
    TEST_ASSERT_TRUE(requestMsg.header.has_messagetype);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_NOOP, requestMsg.header.messagetype);
}
