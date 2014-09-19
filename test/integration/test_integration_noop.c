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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_nbo.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_hmac.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;
static KineticSessionHandle DummyHandle = 1;
static KineticSessionHandle SessionHandle = KINETIC_HANDLE_INVALID;
KineticPDU Request, Response;

// static KineticConnection Connection;
// static KineticPDU Request, Response;
// static KineticOperation Operation;
static KineticPDUHeader RequestHeaderStruct;
static ByteArray RequestHeaderStructArray;
static ByteArray RequestHeader;
static ByteArray ResponseHeaderRaw;
static ByteArray ResponseProtobuf;
// static ByteArray HMACKey;

// Initialize response message status and HMAC
uint8_t HmacData[64];

void setUp(void)
{
    KineticLogger_Init(NULL);
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false; // Ensure gets set appropriately by internal connect call
    Connection.socket = 7;
    HmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("some hmac key");
    LOGF(">>>> HMAC key .len=%u, .data=0x%llX", HmacKey.len, (long long)HmacKey.data);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Request, &Connection);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Response, &Connection);
    KINETIC_SESSION_INIT(&Connection.session, "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Connection.session, DummyHandle);
    KineticConnection_FromHandle_IgnoreAndReturn(&Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Connect(&Connection.session, &SessionHandle);
    TEST_ASSERT_EQUAL_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(DummyHandle, SessionHandle);

    RequestHeaderStruct = KINETIC_PDU_HEADER_INIT;
    RequestHeaderStruct.protobufLength = KineticNBO_FromHostU32(25);
    RequestHeaderStruct.valueLength = KineticNBO_FromHostU32(0);
    RequestHeaderStructArray = (ByteArray){
        .data = (uint8_t*)&RequestHeaderStruct,
        .len = sizeof(RequestHeaderStruct)
    };

    Request.headerNBO = KINETIC_PDU_HEADER_INIT;
    Request.headerNBO.protobufLength = KineticNBO_FromHostU32(25);
    Request.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    RequestHeader = (ByteArray) {
        .data = (uint8_t*)&Request.headerNBO, .len = sizeof(KineticPDUHeader)};
    LOG_LOCATION; KineticLogger_LogHeader(&Request.headerNBO);

    Response.headerNBO = KINETIC_PDU_HEADER_INIT;
    Response.headerNBO.protobufLength = KineticNBO_FromHostU32(17);
    Response.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    ResponseHeaderRaw = (ByteArray) {
        .data = (uint8_t*)&Response.headerNBO, .len = sizeof(KineticPDUHeader)};
    ResponseProtobuf = (ByteArray){.data = Response.protobufRaw};
    Response.connection = &Connection;

    Response.protoData.message.proto.has_hmac = true;
    // Response.protoData.message.proto.hmac.data = HmacKey.data;
    // Response.protoData.message.proto.hmac.len = HmacKey.len;
    Response.proto = &Response.protoData.message.proto;
    LOG("HERE");
    Response.protoData.message.command = (KineticProto_Command)KINETIC_PROTO_COMMAND__INIT; 
    Response.proto->command = &Response.protoData.message.command;
    // Response.value = (ByteArray){.data = NULL};
    LOG("THERE");

}

void tearDown(void)
{
    bool allFreed = KineticAllocator_ValidateAllMemoryFreed();
    if (!allFreed)
    {
        KineticAllocator_FreeAllPDUs();
    }

    KineticConnection_Disconnect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    KineticConnection_FreeConnection_Expect(&DummyHandle);
    KineticStatus status = KineticClient_Disconnect(&SessionHandle);
    
    TEST_ASSERT_TRUE(allFreed);
    TEST_ASSERT_EQUAL_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_NoOp_should_succeed(void)
{   LOG_LOCATION;

    // Response.protoData.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    // Response.protoData.message.command.status = &Response.protoData.message.status;
    // Response.protoData.message.status.has_code = true;

    // KineticHMAC respTempHMAC;
    // KineticHMAC_Populate(&respTempHMAC, &Response.protoData.message.proto, HmacKey);

    LOG_LOCATION; LOGF("  resp->proto: 0x%0llX,\n"
        "resp->con: 0x%0llX, resp->con->session: 0x%0llX\n"
        "resp->con->session->hmacKey: 0x%0llX "
        "hmacKey.data: 0x%0llX, len: %u",
        Response.proto, Response.connection, Response.connection->session,
        &Response.connection->session.hmacKey,
        &Response.connection->session.hmacKey.data,
        &Response.connection->session.hmacKey.len);

    // Send the request
    KineticAllocator_NewPDU_ExpectAndReturn(&Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Response);
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticHMAC_Init_Expect(&Request.hmac,
        KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac,
        Request.proto, Connection.session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, RequestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &Request, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(Connection.socket, ResponseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socket, &Response, true);
    KineticHMAC_Validate_ExpectAndReturn(Response.proto, HmacKey, true);

    // Execute the operation
    KineticStatus status = KineticClient_NoOp(SessionHandle);
    TEST_ASSERT_EQUAL_STATUS(KINETIC_STATUS_SUCCESS, status);
}

#if 0
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

    RequestHeader = (ByteArray){
        .data = (uint8_t*)&Request.headerNBO,
        .len = sizeof(KineticPDUHeader) };
    Response.headerNBO = KINETIC_PDU_HEADER_INIT;
    Response.headerNBO.protobufLength = KineticNBO_FromHostU32(17);
    Response.headerNBO.valueLength = KineticNBO_FromHostU32(0);
    ResponseHeaderRaw = (ByteArray){
        .data = (uint8_t*)&Response.headerNBO,
        .len = sizeof(KineticPDUHeader) };
    ResponseProtobuf = (ByteArray){
        .data = Response.protobufRaw,
        .len = 0};
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
    uint8_t HmacData[64];
    Response.protoData.message.proto.has_hmac = true;
    Response.protoData.message.proto.hmac.data = HmacData;
    KineticHMAC respTempHMAC;
    KineticHMAC_Populate(&respTempHMAC, &Response.protoData.message.proto, HMACKey);

    // Send the request
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticSocket_Write_ExpectAndReturn(Connection.socketDescriptor, RequestHeader, true);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socketDescriptor, &Request, true);

    // Receive the response
    KineticSocket_Read_ExpectAndReturn(Connection.socketDescriptor, ResponseHeaderRaw, true);
    KineticSocket_ReadProtobuf_ExpectAndReturn(Connection.socketDescriptor, &Response, true);

    // Execute the operation
    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS,
        KineticClient_NoOp(&Operation));
}
#endif
