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

#include "unity_helper.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic_nbo.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "kinetic_device_info.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_serial_allocator.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_auth.h"
#include "mock_kinetic_hmac.h"
#include "mock_bus.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_countingsemaphore.h"

static KineticSessionConfig SessionConfig;
static KineticSession Session;
static KineticConnection Connection;
static const int64_t ConnectionID = 12345;
static KineticPDU Request, Response;
static KineticOperation Operation;

void setUp(void)
{
    KineticLogger_Init("stdout", 1);
    KineticSession_Init(&Session, &SessionConfig, &Connection);
    Connection.connectionID = ConnectionID;

    Session.connection = &Connection;
    Connection.pSession = &Session;
    KineticPDU_InitWithCommand(&Request, &Session);
    KineticPDU_InitWithCommand(&Response, &Session);
    KineticOperation_Init(&Operation, &Session);

    Operation.request = &Request;
    Operation.connection = &Connection;
    SessionConfig = (KineticSessionConfig) {.host = "anyhost", .port = KINETIC_PORT};
    Session = (KineticSession) {.config = SessionConfig, .connection = &Connection};
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticOperation_Init_should_configure_the_operation(void)
{
    KineticOperation op = {
        .connection = NULL,
        .request = NULL,
        .response = NULL,
    };

    KineticOperation_Init(&op, &Session);

    TEST_ASSERT_EQUAL_PTR(&Connection, op.connection);
    TEST_ASSERT_NULL(op.request);
    TEST_ASSERT_NULL(op.response);
}


/*******************************************************************************
 * Client Operations
*******************************************************************************/

void test_KineticOperation_SendRequest_should_acquire_and_increment_sequence_count_and_send_PDU_to_bus(void)
{
    TEST_IGNORE_MESSAGE("Need to test KineticOperation_SendRequest");
}

void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    KineticOperation_BuildNoop(&Operation);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP, Request.message.command.header->messageType);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object(void)
{
    ByteArray value = ByteArray_CreateWithCString("Luke, I am your father");
    ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray newVersion = ByteArray_CreateWithCString("v1.0");
    ByteArray tag = ByteArray_CreateWithCString("some_tag");

    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .newVersion = ByteBuffer_CreateWithArray(newVersion),
        .tag = ByteBuffer_CreateWithArray(tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(value),
    };

    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->message, &entry);

    // Build the operation
    KineticOperation_BuildPut(&Operation, &entry);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_TRUE(Operation.sendValue);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry->value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object_with_calculated_tag(void)
{
    ByteArray value = ByteArray_CreateWithCString("Luke, I am your father");
    ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray newVersion = ByteArray_CreateWithCString("v1.0");
    ByteArray tag = ByteArray_CreateWithCString("some_tag");

    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .newVersion = ByteBuffer_CreateWithArray(newVersion),
        .tag = ByteBuffer_CreateWithArray(tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(value),
        .computeTag = true,
    };

    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->message, &entry);

    // Build the operation
    KineticOperation_BuildPut(&Operation, &entry);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_TRUE(Operation.sendValue);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry->value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

uint8_t ValueData[KINETIC_OBJ_SIZE];

void test_KineticOperation_BuildGet_should_build_a_GET_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGet(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.message.command.header->messageType);
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGet_should_build_a_GET_operation_requesting_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGet(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.message.command.header->messageType);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetNext_should_build_a_GETNEXT_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGetNext(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETNEXT,
        Request.message.command.header->messageType);

    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetNext_should_build_a_GETNEXT_operation_with_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGetNext(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETNEXT,
        Request.message.command.header->messageType);

    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetPrevious_should_build_a_GETPREVIOUS_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGetPrevious(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETPREVIOUS,
        Request.message.command.header->messageType);

    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetPrevious_should_build_a_GETPREVIOUS_operation_with_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildGetPrevious(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETPREVIOUS,
        Request.message.command.header->messageType);

    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}



void test_KineticOperation_BuildFlush_should_build_a_FLUSHALLDATA_operation(void)
{
    KineticOperation_BuildFlush(&Operation);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_FLUSHALLDATA,
        Request.message.command.header->messageType);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);

    TEST_ASSERT_NULL(Operation.response);
}



void test_KineticOperation_BuildDelete_should_build_a_DELETE_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {.key = ByteBuffer_CreateWithArray(key), .value = ByteBuffer_CreateWithArray(value)};
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticOperation_BuildDelete(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE, Request.message.command.header->messageType);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}


void test_KineticOperation_BuildGetKeyRange_should_build_a_GetKeyRange_request(void)
{
    const int maxKeyLen = 32; // arbitrary key length for test
    uint8_t startKeyData[maxKeyLen];
    uint8_t endKeyData[maxKeyLen];
    ByteBuffer startKey, endKey;
    startKey = ByteBuffer_Create(startKeyData, sizeof(startKeyData), 0);
    ByteBuffer_AppendCString(&startKey, "key_range_00_00");
    endKey = ByteBuffer_Create(endKeyData, sizeof(endKeyData), 0);
    ByteBuffer_AppendCString(&endKey, "key_range_00_03");
    const int numKeysInRange = 4;
    KineticKeyRange range = {
        .startKey = startKey,
        .endKey = endKey,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = numKeysInRange,
        .reverse = false,
    };

    uint8_t keysData[numKeysInRange][maxKeyLen];
    ByteBuffer keyBuffers[numKeysInRange];
    for (int i = 0; i < numKeysInRange; i++) {
        keyBuffers[i] = ByteBuffer_Create(keysData[i], maxKeyLen, 0);
    }
    ByteBufferArray keys = {.buffers = keyBuffers, .count = numKeysInRange};
    KineticMessage_ConfigureKeyRange_Expect(&Request.message, &range);

    KineticOperation_BuildGetKeyRange(&Operation, &range, &keys);

    TEST_ASSERT_TRUE(Request.command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE, Request.command->header->messageType);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.entry);
    TEST_ASSERT_EQUAL_PTR(&Request, Operation.request);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL_PTR(&Request.message.command, Request.command);
}


void test_KineticOperation_BuildP2POperation_should_build_a_P2POperation_request(void)
{
    ByteBuffer oldKey1 = ByteBuffer_Create((void*)0x1234, 10, 10);
    ByteBuffer newKey1 = ByteBuffer_Create((void*)0x4321, 33, 33);
    ByteBuffer version1 = ByteBuffer_Create((void*)0xABC, 6, 6);

    ByteBuffer oldKey2 = ByteBuffer_Create((void*)0x5678, 12, 12);
    ByteBuffer newKey2 = ByteBuffer_Create((void*)0x8765, 200, 200);

    KineticP2P_OperationData ops2[] ={
        {
            .key = oldKey2,
            .newKey = newKey2,
        }
    };

    KineticP2P_Operation chained_p2pOp = {
        .peer.hostname = "hostname1",
        .peer.port = 4321,
        .numOperations = 1,
        .operations = ops2
    };

    KineticP2P_OperationData ops[] ={
        {
            .key = oldKey1,
            .version = version1,
            .newKey = newKey1,
            .chainedOperation = &chained_p2pOp,
        },
        {
            .key = oldKey2,
            .newKey = newKey2,
        }
    };

    KineticP2P_Operation p2pOp = {
        .peer.hostname = "hostname",
        .peer.port = 1234,
        .peer.tls = true,
        .numOperations = 2,
        .operations = ops
    };
    

    KineticOperation_BuildP2POperation(&Operation, &p2pOp);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PEER2PEERPUSH,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    
    TEST_ASSERT_NOT_NULL(Request.command->body->p2pOperation);

    TEST_ASSERT_EQUAL("hostname",
        Request.command->body->p2pOperation->peer->hostname);

    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->peer->has_port);

    TEST_ASSERT_EQUAL(1234,
        Request.command->body->p2pOperation->peer->port);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->peer->has_tls);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->peer->tls);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->allChildOperationsSucceeded);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->has_allChildOperationsSucceeded);

    TEST_ASSERT_EQUAL(2,
        Request.command->body->p2pOperation->n_operation);

    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->has_key);
    TEST_ASSERT_EQUAL(oldKey1.array.data,
        Request.command->body->p2pOperation->operation[0]->key.data);
    TEST_ASSERT_EQUAL(oldKey1.bytesUsed,
        Request.command->body->p2pOperation->operation[0]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->has_newKey);
    TEST_ASSERT_EQUAL(newKey1.array.data,
        Request.command->body->p2pOperation->operation[0]->newKey.data);
    TEST_ASSERT_EQUAL(newKey1.bytesUsed,
        Request.command->body->p2pOperation->operation[0]->newKey.len);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->has_version);
    TEST_ASSERT_EQUAL(version1.array.data,
        Request.command->body->p2pOperation->operation[0]->version.data);
    TEST_ASSERT_EQUAL(version1.bytesUsed,
        Request.command->body->p2pOperation->operation[0]->version.len);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->operation[0]->has_force);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->operation[0]->force);

    TEST_ASSERT_NOT_NULL(Request.command->body->p2pOperation->operation[0]->p2pop);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->peer->has_port);
    TEST_ASSERT_EQUAL(4321,
        Request.command->body->p2pOperation->operation[0]->p2pop->peer->port);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->peer->has_tls);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->operation[0]->p2pop->peer->tls);
    TEST_ASSERT_EQUAL(1,
        Request.command->body->p2pOperation->operation[0]->p2pop->n_operation);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->has_key);
    TEST_ASSERT_EQUAL(oldKey2.array.data,
        Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->key.data);
    TEST_ASSERT_EQUAL(oldKey2.bytesUsed,
        Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->has_newKey);
    TEST_ASSERT_EQUAL(newKey2.array.data,
        Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->newKey.data);
    TEST_ASSERT_EQUAL(newKey2.bytesUsed,
        Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->newKey.len);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->has_version);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->has_force);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[0]->p2pop->operation[0]->force);
    TEST_ASSERT_NULL(Request.command->body->p2pOperation->operation[0]->status);

    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[1]->has_key);
    TEST_ASSERT_EQUAL(oldKey2.array.data,
        Request.command->body->p2pOperation->operation[1]->key.data);
    TEST_ASSERT_EQUAL(oldKey2.bytesUsed,
        Request.command->body->p2pOperation->operation[1]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[1]->has_newKey);
    TEST_ASSERT_EQUAL(newKey2.array.data,
        Request.command->body->p2pOperation->operation[1]->newKey.data);
    TEST_ASSERT_EQUAL(newKey2.bytesUsed,
        Request.command->body->p2pOperation->operation[1]->newKey.len);
    TEST_ASSERT_FALSE(Request.command->body->p2pOperation->operation[1]->has_version);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[1]->has_force);
    TEST_ASSERT_TRUE(Request.command->body->p2pOperation->operation[1]->force);
    TEST_ASSERT_NULL(Request.command->body->p2pOperation->operation[1]->p2pop);
    TEST_ASSERT_NULL(Request.command->body->p2pOperation->operation[1]->status);

    TEST_ASSERT_EQUAL_PTR(&p2pOp, Operation.p2pOp);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);

    TEST_ASSERT_NULL(Operation.response);

    // This just free's the malloc'd memory and sets statuses to "invalid" (since no operation was actually performed)
    KineticOperation_P2POperationCallback(&Operation, KINETIC_STATUS_SUCCESS);
}



/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/

void test_KineticOperation_BuildSetPin_should_build_a_SECURITY_operation_to_set_new_LOCK_PIN(void)
{
    char oldPinData[] = "1234";
    char newPinData[] = "5678";
    ByteArray oldPin = ByteArray_Create(oldPinData, sizeof(oldPinData));
    ByteArray newPin = ByteArray_Create(newPinData, sizeof(newPinData));
    KineticOperation_BuildSetPin(&Operation, oldPin, newPin, true);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SECURITY,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.security, Request.command->body->security);
    TEST_ASSERT_TRUE(Request.command->body->security->has_oldLockPIN);
    TEST_ASSERT_EQUAL_PTR(oldPinData, Request.command->body->security->oldLockPIN.data);
    TEST_ASSERT_EQUAL(oldPin.len, Request.command->body->security->oldLockPIN.len);
    TEST_ASSERT_TRUE(Request.command->body->security->has_newLockPIN);
    TEST_ASSERT_EQUAL_PTR(newPinData, Request.command->body->security->newLockPIN.data);
    TEST_ASSERT_EQUAL(newPin.len, Request.command->body->security->newLockPIN.len);
    TEST_ASSERT_NULL(Request.command->body->pinOp);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_SetPinCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
}
void test_KineticOperation_BuildSetPin_should_build_a_SECURITY_operation_to_set_new_ERASE_PIN(void)
{
    char oldPinData[] = "1234";
    char newPinData[] = "5678";
    ByteArray oldPin = ByteArray_Create(oldPinData, sizeof(oldPinData));
    ByteArray newPin = ByteArray_Create(newPinData, sizeof(newPinData));
    KineticOperation_BuildSetPin(&Operation, oldPin, newPin, false);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SECURITY,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.security, Request.command->body->security);
    TEST_ASSERT_TRUE(Request.command->body->security->has_oldErasePIN);
    TEST_ASSERT_EQUAL_PTR(oldPinData, Request.command->body->security->oldErasePIN.data);
    TEST_ASSERT_EQUAL(oldPin.len, Request.command->body->security->oldErasePIN.len);
    TEST_ASSERT_TRUE(Request.command->body->security->has_newErasePIN);
    TEST_ASSERT_EQUAL_PTR(newPinData, Request.command->body->security->newErasePIN.data);
    TEST_ASSERT_EQUAL(newPin.len, Request.command->body->security->newErasePIN.len);
    TEST_ASSERT_NULL(Request.command->body->pinOp);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_SetPinCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
}



void test_KineticOperation_BuildGetLog_should_build_a_GetLog_request(void)
{
    KineticDeviceInfo* pInfo;

    KineticOperation_BuildGetLog(&Operation, KINETIC_DEVICE_INFO_TYPE_STATISTICS, &pInfo);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETLOG,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.getLog, Request.command->body->getLog);
    TEST_ASSERT_NOT_NULL(Request.command->body->getLog->types);
    TEST_ASSERT_EQUAL(1, Request.command->body->getLog->n_types);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_STATISTICS,
        Request.command->body->getLog->types[0]);
    TEST_ASSERT_EQUAL_PTR(&pInfo, Operation.deviceInfo);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}


// void test_KineticOperation_GetLogCallback_should_copy_returned_device_info_into_dynamically_allocated_info_structure(void)
// {
//     // KineticConnection con;
//     // KineticPDU response;
//     // KineticDeviceInfo* info;
//     // KineticOperation op = {
//     //     .connection = &con,
//     //     .response = &response,
//     //     .deviceInfo = &info,
//     // };

//     TEST_IGNORE_MESSAGE("TODO: Need to implement!")

//     // KineticSerialAllocator_Create()

//     // KineticStatus status = KineticOperation_GetLogCallback(&op);
// }



void test_KineticOperation_BuildErase_should_build_a_SECURE_ERASE_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_BuildErase(&Operation, true, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinOp);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinOpType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_SECURE_ERASE_PINOP,
        Request.command->body->pinOp->pinOpType);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_EraseCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(180, Operation.timeoutSeconds);
}

void test_KineticOperation_BuildErase_should_build_an_INSTANT_ERASE_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_BuildErase(&Operation, false, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinOp);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinOpType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_ERASE_PINOP,
        Request.command->body->pinOp->pinOpType);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_EraseCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(180, Operation.timeoutSeconds);
}



void test_KineticOperation_BuildLockUnlock_should_build_a_LOCK_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_BuildLockUnlock(&Operation, true, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinOp);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinOpType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_LOCK_PINOP,
        Request.command->body->pinOp->pinOpType);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_LockUnlockCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
}

void test_KineticOperation_BuildLockUnlock_should_build_an_UNLOCK_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_BuildLockUnlock(&Operation, false, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PINOP,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinOp);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinOpType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_PIN_OPERATION_PIN_OP_TYPE_UNLOCK_PINOP,
        Request.command->body->pinOp->pinOpType);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_LockUnlockCallback, Operation.callback);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
}



void test_KineticOperation_BuildSetClusterVersion_should_build_a_SET_CLUSTER_VERSION_operation_with_PIN_auth(void)
{
    KineticOperation_BuildSetClusterVersion(&Operation, 1776);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_SETUP,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);

    TEST_ASSERT_EQUAL_PTR(&Request.message.setup, Request.command->body->setup);
    TEST_ASSERT_EQUAL_INT64(1776, Request.message.setup.newClusterVersion);
    TEST_ASSERT_TRUE(Request.message.setup.has_newClusterVersion);
    TEST_ASSERT_FALSE(Request.message.setup.has_firmwareDownload);
    TEST_ASSERT_EQUAL_PTR(&KineticOperation_SetClusterVersionCallback, Operation.callback);
}
