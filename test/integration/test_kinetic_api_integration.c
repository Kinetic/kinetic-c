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
    KineticApi_Init(NULL);
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    const int64_t identity = 1234;
    uint8_t key[] = {1,2,3};
    const int socketDesc = 783;
    KineticConnection connection = {
        .socketDescriptor = socketDesc // Fill in, since KineticConnection is mocked
    };
    KineticMessage requestMsg, responseMsg;
    KineticProto_Status_StatusCode status;
    KineticHMAC respTempHMAC;

    KineticConnection_Init_Expect(&connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "localhost", 8999, true, true);

    TEST_ASSERT_TRUE(KineticApi_Connect(&connection, "localhost", 8999, true));

    TEST_ASSERT_EQUAL_INT(socketDesc, connection.socketDescriptor); // Ensure socket descriptor still intact!

    TEST_ASSERT_TRUE_MESSAGE(
        KineticApi_ConfigureExchange(&exchange, &connection, identity,
            key, sizeof(key)),
        "Failed configuring exchange!");

    operation = KineticApi_CreateOperation(&exchange, &request, &requestMsg, &response, &responseMsg);

    // Initialize response message status and HMAC, since receipt of packed protobuf is mocked out
    responseMsg.command.status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    KineticHMAC_Populate(&respTempHMAC, &responseMsg, key, sizeof(key));

    KineticSocket_Write_ExpectAndReturn(socketDesc, &request.header, sizeof(KineticPDUHeader), true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(socketDesc, &requestMsg, true);
    KineticSocket_Read_ExpectAndReturn(socketDesc, &response.header, sizeof(KineticPDUHeader), true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(socketDesc, &responseMsg.proto, response.protobufScratch, 0x08000000, true);

    status = KineticApi_NoOp(&operation);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
}
