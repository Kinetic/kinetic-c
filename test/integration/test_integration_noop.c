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
#include "kinetic_types.h"
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


static KineticConnection Connection;
static KineticPDU Request, Response;
static KineticOperation Operation;
static ByteArray RequestHeader;
static ByteArray ResponseHeaderRaw;
static ByteArray ResponseProtobuf;
static ByteArray HMACKey;

void setUp(void)
{
    const char* host = "localhost";
    const int port = 8899;
    const int64_t clusterVersion = 9876;
    const int64_t identity = 1234;
    const int socketDesc = 783;
    HMACKey = BYTE_ARRAY_INIT_FROM_CSTRING("some_hmac_key");

    KineticClient_Init(NULL);

    // Establish Connection
    KINETIC_CONNECTION_INIT(&Connection, identity, HMACKey);
    Connection.socketDescriptor = socketDesc;
    KineticConnection_Connect_ExpectAndReturn(&Connection,
            host, port, false, clusterVersion, identity, HMACKey, true);

    bool success = KineticClient_Connect(&Connection,
                                         host, port, false, clusterVersion, identity, HMACKey);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(socketDesc, Connection.socketDescriptor);

    // Create the operation
    Operation = KineticClient_CreateOperation(&Connection, &Request, &Response);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Request, &Connection);
    KineticLogger_LogProtobuf(Request.proto);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Response, &Connection);
    KineticLogger_LogProtobuf(Response.proto);

    RequestHeader = (ByteArray) {
        .data = (uint8_t*)&Request.headerNBO,
         .len = sizeof(KineticPDUHeader)
    };
    Response.headerNBO = KINETIC_PDU_HEADER_INIT;
    Response.headerNBO.protobufLength = KineticNBO_FromHostU32(17);
    Response.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    ResponseHeaderRaw = (ByteArray) {
        .data = (uint8_t*)&Response.headerNBO,
         .len = sizeof(KineticPDUHeader)
    };
    ResponseProtobuf = (ByteArray) {
        .data = Response.protobufRaw,
         .len = 0
    };
}

void tearDown(void)
{
}

void test_NoOp_should_succeed(void)
{
    LOG_LOCATION;
    Response.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    Response.protoData.message.command.status = &Response.protoData.message.status;
    Response.protoData.message.status.has_code = true;

    // Initialize response message status and HMAC
    uint8_t hmacData[64];
    Response.protoData.message.proto.has_hmac = true;
    Response.protoData.message.proto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &Response.protoData.message.proto, HMACKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, RequestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &Request, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, ResponseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &Response, true);

    // // Execute the operation
    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS,
        KineticClient_NoOp(&Operation));
}
