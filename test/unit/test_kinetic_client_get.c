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
#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include <stdio.h>
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;
static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey, Key, Tag, Value;
static uint8_t ValueData[64];
static KineticSessionHandle DummyHandle = 1;
static KineticSessionHandle SessionHandle = KINETIC_HANDLE_INVALID;
KineticPDU Request, Response;


void setUp(void)
{
    KineticLogger_Init("stdout", 3);

    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false;
    HmacKey = ByteArray_CreateWithCString("some hmac key");
    Key = ByteArray_CreateWithCString("some_value_key");
    Tag = ByteArray_CreateWithCString("some_tag_value");
    Value = ByteArray_CreateWithCString("some random abitrary data...");
    KINETIC_SESSION_INIT(&Session,
                         "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Session, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    KineticConnection_ReceiveDeviceStatusMessage_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    
    KineticStatus status = KineticClient_Connect(&Session, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(DummyHandle, SessionHandle);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_Get_should_execute_GET_operation(void)
{
    LOG_LOCATION;

    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ByteBuffer valueBuffer = ByteBuffer_CreateWithArray(Value);
    uint8_t versionData[128];
    ByteBuffer versionBuffer = ByteBuffer_Create(versionData, sizeof(versionData), 5);
    versionData[0] = 9;
    versionData[1] = 8;
    versionData[2] = 7;
    versionData[3] = 6;
    versionData[4] = 0xFF;
    uint8_t keyData[128];
    ByteBuffer keyBuffer = ByteBuffer_Create(keyData, sizeof(keyData), 5);
    keyData[0] = 5;
    keyData[1] = 4;
    keyData[2] = 3;
    keyData[3] = 2;
    keyData[4] = 1;

    KineticEntry reqEntry = {
        .key = keyBuffer,
        .tag = ByteBuffer_CreateWithArray(Tag),
        .dbVersion = versionBuffer,
        .value = valueBuffer,
    };

    KineticProto_Command_KeyValue keyValue = KINETIC_PROTO_COMMAND_KEY_VALUE__INIT;
    keyValue.key = (ProtobufCBinaryData) {
        .data = (uint8_t*)keyData,
         .len = 3,
    };
    keyValue.has_key = true;

    uint8_t respFakeVer[] = {12, 13, 14, 15, 16, 17};
    keyValue.dbVersion = (ProtobufCBinaryData) {
        .data = respFakeVer,
         .len = sizeof(respFakeVer),
    };
    keyValue.has_dbVersion = true;

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &reqEntry);
    KineticPDU_Send_ExpectAndReturn(&Request, KINETIC_STATUS_SUCCESS);
    KineticPDU_Receive_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetKeyValue_ExpectAndReturn(&Response, &keyValue);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Request);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Response);

    KineticStatus status = KineticClient_Get(DummyHandle, &reqEntry);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    KineticLogger_LogByteBuffer(1, "key", reqEntry.key);
    KineticLogger_LogByteBuffer(1, "dbVersion", reqEntry.dbVersion);
    KineticLogger_LogByteBuffer(1, "value", reqEntry.value);
}

void test_KineticClient_Get_should_execute_GET_operation_and_populate_supplied_buffer_with_value(void)
{
    LOG_LOCATION;

    uint8_t keyData[128];
    ByteBuffer keyBuffer = ByteBuffer_Create(keyData, sizeof(keyData), 5);
    keyData[0] = 5;
    keyData[1] = 4;
    keyData[2] = 3;
    keyData[3] = 2;
    keyData[4] = 1;

    uint8_t versionData[128];
    ByteBuffer versionBuffer = ByteBuffer_Create(versionData, sizeof(versionData), 5);
    versionData[0] = 9;
    versionData[1] = 8;
    versionData[2] = 7;
    versionData[3] = 6;
    versionData[4] = 0xFF;

    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ByteBuffer valueBuffer = ByteBuffer_CreateWithArray(Value);
    ByteBuffer_AppendDummyData(&valueBuffer, Value.len);

    KineticEntry reqEntry = {
        .key = keyBuffer,
        .tag = ByteBuffer_CreateWithArray(Tag),
        .dbVersion = versionBuffer,
        .value = valueBuffer,
    };

    KineticProto_Command_KeyValue keyValue = KINETIC_PROTO_COMMAND_KEY_VALUE__INIT;
    keyValue.key = (ProtobufCBinaryData){.data = (uint8_t*)keyData, .len = 3};
    keyValue.has_key = true;

    uint8_t respFakeVer[] = {12, 13, 14, 15, 16, 17};
    keyValue.dbVersion = (ProtobufCBinaryData){.data = respFakeVer, .len = sizeof(respFakeVer)};
    keyValue.has_dbVersion = true;

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &reqEntry);
    KineticPDU_Send_ExpectAndReturn(&Request, KINETIC_STATUS_SUCCESS);
    KineticPDU_Receive_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetKeyValue_ExpectAndReturn(&Response, &keyValue);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Request);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Response);

    KineticStatus status = KineticClient_Get(DummyHandle, &reqEntry);

    KineticLogger_LogByteBuffer(0, "key", reqEntry.key);
    KineticLogger_LogByteBuffer(0, "dbVersion", reqEntry.dbVersion);
    KineticLogger_LogByteBuffer(0, "value", reqEntry.value);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_Get_should_execute_GET_operation_and_retrieve_only_metadata(void)
{
    LOG_LOCATION;

    uint8_t keyData[128];
    ByteBuffer keyBuffer = ByteBuffer_Create(keyData, sizeof(keyData), 5);
    keyData[0] = 5;
    keyData[1] = 4;
    keyData[2] = 3;
    keyData[3] = 2;
    keyData[4] = 1;

    uint8_t versionData[128];
    ByteBuffer versionBuffer = ByteBuffer_Create(versionData, sizeof(versionData), 5);
    versionData[0] = 9;
    versionData[1] = 8;
    versionData[2] = 7;
    versionData[3] = 6;
    versionData[4] = 0xFF;

    KineticEntry reqEntry = {
        .key = keyBuffer,
        .tag = ByteBuffer_CreateWithArray(Tag),
        .dbVersion = versionBuffer,
        .metadataOnly = true,
    };

    KineticProto_Command_KeyValue keyValue = KINETIC_PROTO_COMMAND_KEY_VALUE__INIT;
    keyValue.key = (ProtobufCBinaryData){.data = (uint8_t*)keyData, .len = 3};
    keyValue.has_key = true;

    uint8_t respFakeVer[] = {12, 13, 14, 15, 16, 17};
    keyValue.dbVersion = (ProtobufCBinaryData){.data = respFakeVer, .len = sizeof(respFakeVer)};
    keyValue.has_dbVersion = true;

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &reqEntry);
    KineticPDU_Send_ExpectAndReturn(&Request, KINETIC_STATUS_SUCCESS);
    KineticPDU_Receive_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetKeyValue_ExpectAndReturn(&Response, &keyValue);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Request);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Response);

    KineticStatus status = KineticClient_Get(DummyHandle, &reqEntry);

    KineticLogger_LogByteBuffer(0, "key", reqEntry.key);
    KineticLogger_LogByteBuffer(0, "dbVersion", reqEntry.dbVersion);
    KineticLogger_LogByteBuffer(0, "value", reqEntry.value);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
