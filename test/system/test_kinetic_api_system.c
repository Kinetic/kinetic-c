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
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"

#include "unity.h"
#include "unity_helper.h"
#include <protobuf-c/protobuf-c.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t identity = 1000;
    const char key[] = "foobarbaz";
    KineticConnection connection;
    KineticMessage requestMsg, responseMsg;
    KineticProto_Status_StatusCode status;
    bool success;

    KineticApi_Init(NULL);

    success = KineticApi_Connect(&connection, "localhost", KINETIC_PORT, true);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT(connection.socketDescriptor >= 0);

    success = KineticApi_ConfigureExchange(&exchange, &connection, identity,
            key, strlen(key));
    TEST_ASSERT_TRUE(success);

    operation = KineticApi_CreateOperation(&exchange, &request, &requestMsg,
        &response, &responseMsg);

    status = KineticApi_NoOp(&operation);

    TEST_IGNORE_MESSAGE("Need to track down why the Java simulator is not responding to NOOP request!");

    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS,
        status);
}
