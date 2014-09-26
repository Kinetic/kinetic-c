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
#include <stdio.h>
#include "protobuf-c/protobuf-c.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"


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

void test_Get_should_retrieve_an_object_from_the_device_and_store_in_supplied_byte_array(void)
{
    LOG_LOCATION;
    Response.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    Response.protoData.message.command.status = &Response.protoData.message.status;
    Response.protoData.message.status.has_code = true;

    // Create operation-specific data and paylod
    ByteArray valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum...");
    KineticKeyValue metadata = {
        .key = valueKey,
        .value = value,
    };

    // Initialize response message status and HMAC
    uint8_t hmacData[64];
    Response.protoData.message.proto.has_hmac = true;
    Response.protoData.message.proto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &Response.protoData.message.proto, HMACKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor,
                                        RequestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor,
            &Request, true);

    // Receive the response
    Response.headerNBO.valueLength = KineticNBO_FromHostU32(value.len);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
                                       ResponseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor,
            &Response, true);
    LOG_LOCATION; LOGF("value.len=%u, value.data=0x%zu", value.len, value.data);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
                                       value, true);

    // Execute the operation
    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS,
        KineticClient_Get(&Operation, &metadata));
    TEST_ASSERT_EQUAL_PTR(value.data, Response.value.data);
}

void test_Get_should_create_retrieve_an_object_from_the_device_and_store_in_embedded_ByteArray(void)
{
    LOG_LOCATION;
    Response.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    Response.protoData.message.command.status = &Response.protoData.message.status;
    Response.protoData.message.status.has_code = true;

    // Create operation-specific data and paylod
    ByteArray valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    const ByteArray const expectedValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum...");

    // Associate the dummy value data with the response
    KineticPDU_EnableValueBuffer(&Response);
    memcpy(Response.valueBuffer, expectedValue.data, expectedValue.len);

    // Initialize response message status and HMAC
    uint8_t hmacData[64];
    Response.protoData.message.proto.has_hmac = true;
    Response.protoData.message.proto.hmac.data = hmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &Response.protoData.message.proto, HMACKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor,
                                        RequestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor,
            &Request, true);

    // Receive the response
    Response.headerNBO.valueLength = KineticNBO_FromHostU32(expectedValue.len);
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
                                       ResponseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor,
            &Response, true);
    ByteArray value = {.data = Response.value.data, .len = expectedValue.len};
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor,
                                       value, true);

    // Execute the operation
    KineticKeyValue metadata = {.key = valueKey};
    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS,
        KineticClient_Get(&Operation, &metadata));
    TEST_ASSERT_EQUAL_PTR(metadata.value.data, Response.value.data);
}
