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
#include "protobuf-c/protobuf-c.h"
#include "kinetic_proto.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "kinetic_logger.h"
#include "mock_kinetic_operation.h"

static KineticPDU Request, Response;

void setUp(void)
{
    KineticLogger_Init(NULL);
}

void tearDown(void)
{
}

void test_KineticClient_Get_should_execute_GET_operation_and_populate_supplied_buffer_with_value(void)
{
    KineticConnection connection;
    KINETIC_PDU_INIT(&Request, &connection);
    KINETIC_PDU_INIT(&Response, &connection);
    KineticOperation operation;
    KineticProto_Status_StatusCode status;

    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("How I wish... How I wish you were here...");
    Kinetic_KeyValue metadata = {
        .key = key,
        .tag = tag,
    };

    Request.connection = &connection;

    Response.message.status.has_code = true;
    Response.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    KINETIC_CONNECTION_INIT(&connection, 12, key);
    KineticPDU_Init_Expect(&Request, &connection);
    KineticPDU_Init_Expect(&Response, &connection);
    operation = KineticClient_CreateOperation(&connection, &Request, &Response);

    KineticOperation_BuildGet_Expect(&operation, &metadata, value);
    KineticPDU_Send_ExpectAndReturn(&Request, true);
    KineticPDU_Receive_ExpectAndReturn(&Response, true);
    KineticPDU_Status_ExpectAndReturn(&Response, KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS);

    status = KineticClient_Get(&operation, &metadata, value);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, Response.connection);
}

void test_KineticClient_Get_should_execute_GET_operation_and_populate_embedded_PDU_buffer_for_value_if_none_supplied(void)
{
    KineticConnection connection;
    KINETIC_PDU_INIT(&Request, &connection);
    KINETIC_PDU_INIT(&Response, &connection);
    KineticOperation operation;
    KineticProto_Status_StatusCode status;

    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    Kinetic_KeyValue metadata = {
        .key = key,
        .tag = tag,
    };
    Request.connection = &connection;

    Response.message.status.has_code = true;
    Response.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    KINETIC_CONNECTION_INIT(&connection, 12, key);
    KineticPDU_Init_Expect(&Request, &connection);
    KineticPDU_Init_Expect(&Response, &connection);
    operation = KineticClient_CreateOperation(&connection, &Request, &Response);

    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("We're just 2 lost souls swimming in a fish bowl.. year after year...");
    memcpy(Response.valueBuffer, value.data, value.len);
    ByteArray expectedValue = {.data = Response.valueBuffer, .len = 0};
    KineticOperation_BuildGet_Expect(&operation, &metadata, expectedValue);
    KineticPDU_Send_ExpectAndReturn(&Request, true);
    KineticPDU_Receive_ExpectAndReturn(&Response, true);
    KineticPDU_Status_ExpectAndReturn(&Response, KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS);

    status = KineticClient_Get(&operation, &metadata, BYTE_ARRAY_NONE);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, Response.connection);
}

void test_KineticClient_Get_should_execute_GET_operation_and_retrieve_only_metadata(void)
{
    KineticConnection connection;
    KineticOperation operation;
    KineticProto_Status_StatusCode status;

    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");
    Kinetic_KeyValue metadata = {
        .key = key,
        .tag = tag,
        .metadataOnly = true,
    };

    Request.connection = &connection;
    
    Response.message.status.has_code = true;
    Response.message.status.code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;

    KINETIC_CONNECTION_INIT(&connection, 12, key);
    KineticPDU_Init_Expect(&Request, &connection);
    KineticPDU_Init_Expect(&Response, &connection);
    operation = KineticClient_CreateOperation(&connection, &Request, &Response);

    KineticOperation_BuildGet_Expect(&operation, &metadata, BYTE_ARRAY_NONE);
    KineticPDU_Send_ExpectAndReturn(&Request, true);
    KineticPDU_Receive_ExpectAndReturn(&Response, true);
    KineticPDU_Status_ExpectAndReturn(&Response, KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS);

    status = KineticClient_Get(&operation, &metadata, BYTE_ARRAY_NONE);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, Response.connection);
}
