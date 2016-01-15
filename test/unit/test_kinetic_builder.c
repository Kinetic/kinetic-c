/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "unity_helper.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "kinetic_builder.h"
#include "kinetic_memory.h"
#include "kinetic_allocator.h"
#include "mock_kinetic_resourcewaiter.h"
#include "mock_kinetic_callbacks.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_message.h"

static KineticSession Session;
static KineticRequest Request;
static KineticOperation Operation;

void setUp(void)
{
    KineticLogger_Init("stdout", 1);
    Session = (KineticSession) {.connected = true};
    Operation = (KineticOperation) {.session = &Session};
    KineticRequest_Init(&Request, &Session);
    Operation.request = &Request;
}

void tearDown(void)
{
    KineticLogger_Close();
}

/*******************************************************************************
 * Client Operations
*******************************************************************************/

void test_KineticBuilder_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildNoop(&Operation);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__NOOP, Request.message.command.header->messagetype);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticBuilder_BuildPut_should_return_BUFFER_OVERRUN_if_object_value_too_long(void)
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
    entry.value.bytesUsed = KINETIC_OBJ_SIZE + 1;

    KineticOperation_ValidateOperation_Expect(&Operation);

    // Build the operation
    KineticStatus status = KineticBuilder_BuildPut(&Operation, &entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);
}

void test_KineticBuilder_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object(void)
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

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->message, &entry);

    // Build the operation
    KineticBuilder_BuildPut(&Operation, &entry);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL_PTR(entry.value.array.data, Operation.value.data);
    TEST_ASSERT_EQUAL(entry.value.bytesUsed, Operation.value.len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PUT,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry->value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticBuilder_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object_with_calculated_tag(void)
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

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->message, &entry);

    // Build the operation
    KineticBuilder_BuildPut(&Operation, &entry);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL_PTR(entry.value.array.data, Operation.value.data);
    TEST_ASSERT_EQUAL(entry.value.bytesUsed, Operation.value.len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PUT,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry->value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

uint8_t ValueData[KINETIC_OBJ_SIZE];

void test_KineticBuilder_BuildGet_should_build_a_GET_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGet(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GET, Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticBuilder_BuildGet_should_build_a_GET_operation_requesting_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGet(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GET, Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticBuilder_BuildGetNext_should_build_a_GETNEXT_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGetNext(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETNEXT,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticBuilder_BuildGetNext_should_build_a_GETNEXT_operation_with_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGetNext(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETNEXT,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticBuilder_BuildGetPrevious_should_build_a_GETPREVIOUS_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGetPrevious(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETPREVIOUS,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticBuilder_BuildGetPrevious_should_build_a_GETPREVIOUS_operation_with_metadata_only(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildGetPrevious(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETPREVIOUS,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Get, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}



void test_KineticBuilder_BuildFlush_should_build_a_FLUSHALLDATA_operation(void)
{
    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildFlush(&Operation);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__FLUSHALLDATA,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);

    TEST_ASSERT_NULL(Operation.response);
}



void test_KineticBuilder_BuildDelete_should_build_a_DELETE_operation(void)
{
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {.key = ByteBuffer_CreateWithArray(key), .value = ByteBuffer_CreateWithArray(value)};

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

    KineticBuilder_BuildDelete(&Operation, &entry);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__DELETE,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_Delete, Operation.opCallback);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}


void test_KineticBuilder_BuildGetKeyRange_should_build_a_GetKeyRange_request(void)
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

    KineticOperation_ValidateOperation_Expect(&Operation);
    KineticMessage_ConfigureKeyRange_Expect(&Request.message, &range);

    KineticBuilder_BuildGetKeyRange(&Operation, &range, &keys);

    TEST_ASSERT_TRUE(Request.command->header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETKEYRANGE,
        Request.command->header->messagetype);
    TEST_ASSERT_EQUAL_PTR(KineticCallbacks_GetKeyRange, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.entry);
    TEST_ASSERT_EQUAL_PTR(&Request, Operation.request);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL_PTR(&Request.message.command, Request.command);
}


void test_KineticBuilder_BuildP2POperation_should_build_a_P2POperation_request(void)
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
        .peer = {
            .hostname = "hostname1",
            .port = 4321,
        },
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
        .peer = {
            .hostname = "hostname",
            .port = 1234,
            .tls = true,
        },
        .numOperations = 2,
        .operations = ops
    };

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildP2POperation(&Operation, &p2pOp);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PEER2PEERPUSH,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);

    TEST_ASSERT_NOT_NULL(Request.command->body->p2poperation);

    TEST_ASSERT_EQUAL("hostname",
        Request.command->body->p2poperation->peer->hostname);

    TEST_ASSERT_TRUE(Request.command->body->p2poperation->peer->has_port);

    TEST_ASSERT_EQUAL(1234,
        Request.command->body->p2poperation->peer->port);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->peer->has_tls);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->peer->tls);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->allchildoperationssucceeded);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->has_allchildoperationssucceeded);

    TEST_ASSERT_EQUAL(2,
        Request.command->body->p2poperation->n_operation);

    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->has_key);
    TEST_ASSERT_EQUAL(oldKey1.array.data,
        Request.command->body->p2poperation->operation[0]->key.data);
    TEST_ASSERT_EQUAL(oldKey1.bytesUsed,
        Request.command->body->p2poperation->operation[0]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->has_newkey);
    TEST_ASSERT_EQUAL(newKey1.array.data,
        Request.command->body->p2poperation->operation[0]->newkey.data);
    TEST_ASSERT_EQUAL(newKey1.bytesUsed,
        Request.command->body->p2poperation->operation[0]->newkey.len);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->has_version);
    TEST_ASSERT_EQUAL(version1.array.data,
        Request.command->body->p2poperation->operation[0]->version.data);
    TEST_ASSERT_EQUAL(version1.bytesUsed,
        Request.command->body->p2poperation->operation[0]->version.len);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->operation[0]->has_force);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->operation[0]->force);

    TEST_ASSERT_NOT_NULL(Request.command->body->p2poperation->operation[0]->p2pop);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->peer->has_port);
    TEST_ASSERT_EQUAL(4321,
        Request.command->body->p2poperation->operation[0]->p2pop->peer->port);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->peer->has_tls);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->operation[0]->p2pop->peer->tls);
    TEST_ASSERT_EQUAL(1,
        Request.command->body->p2poperation->operation[0]->p2pop->n_operation);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->has_key);
    TEST_ASSERT_EQUAL(oldKey2.array.data,
        Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->key.data);
    TEST_ASSERT_EQUAL(oldKey2.bytesUsed,
        Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->has_newkey);
    TEST_ASSERT_EQUAL(newKey2.array.data,
        Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->newkey.data);
    TEST_ASSERT_EQUAL(newKey2.bytesUsed,
        Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->newkey.len);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->has_version);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->has_force);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[0]->p2pop->operation[0]->force);
    TEST_ASSERT_NULL(Request.command->body->p2poperation->operation[0]->status);

    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[1]->has_key);
    TEST_ASSERT_EQUAL(oldKey2.array.data,
        Request.command->body->p2poperation->operation[1]->key.data);
    TEST_ASSERT_EQUAL(oldKey2.bytesUsed,
        Request.command->body->p2poperation->operation[1]->key.len);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[1]->has_newkey);
    TEST_ASSERT_EQUAL(newKey2.array.data,
        Request.command->body->p2poperation->operation[1]->newkey.data);
    TEST_ASSERT_EQUAL(newKey2.bytesUsed,
        Request.command->body->p2poperation->operation[1]->newkey.len);
    TEST_ASSERT_FALSE(Request.command->body->p2poperation->operation[1]->has_version);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[1]->has_force);
    TEST_ASSERT_TRUE(Request.command->body->p2poperation->operation[1]->force);
    TEST_ASSERT_NULL(Request.command->body->p2poperation->operation[1]->p2pop);
    TEST_ASSERT_NULL(Request.command->body->p2poperation->operation[1]->status);

    TEST_ASSERT_EQUAL_PTR(&p2pOp, Operation.p2pOp);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);

    TEST_ASSERT_NULL(Operation.response);

    // This just free's the malloc'd memory and sets statuses to "invalid" (since no operation was actually performed)
    KineticAllocator_FreeP2PProtobuf(Request.command->body->p2poperation);
}



/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/

void test_KineticBuilder_BuildGetLog_should_build_a_GetLog_request(void)
{
    KineticLogInfo* pInfo;

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticStatus status = KineticBuilder_BuildGetLog(&Operation,
        KINETIC_DEVICE_INFO_TYPE_STATISTICS, BYTE_ARRAY_NONE, &pInfo);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETLOG,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.getLog, Request.command->body->getlog);
    TEST_ASSERT_NOT_NULL(Request.command->body->getlog->types);
    TEST_ASSERT_EQUAL(1, Request.command->body->getlog->n_types);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__STATISTICS,
        Request.command->body->getlog->types[0]);
    TEST_ASSERT_EQUAL_PTR(&pInfo, Operation.deviceInfo);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticBuilder_BuildGetLog_should_build_a_GetLog_request_to_retrieve_device_specific_log_info(void)
{
    KineticLogInfo* pInfo;
    const char nameData[] = "com.WD";
    ByteArray name = ByteArray_CreateWithCString(nameData);

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticStatus status = KineticBuilder_BuildGetLog(&Operation, COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE, name, &pInfo);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__GETLOG,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.getLog, Request.command->body->getlog);
    TEST_ASSERT_NOT_NULL(Request.command->body->getlog->types);
    TEST_ASSERT_EQUAL(1, Request.command->body->getlog->n_types);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE,
        Request.command->body->getlog->types[0]);
    TEST_ASSERT_EQUAL_PTR(&Request.message.getLogDevice, Request.command->body->getlog->device);
    TEST_ASSERT_TRUE(Request.command->body->getlog->device->has_name);
    TEST_ASSERT_EQUAL_PTR(nameData, Request.command->body->getlog->device->name.data);
    TEST_ASSERT_EQUAL(strlen(nameData), Request.command->body->getlog->device->name.len);
    TEST_ASSERT_EQUAL_PTR(&pInfo, Operation.deviceInfo);
    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticBuilder_BuildGetLog_should_return_KINETIC_STATUS_DEVICE_NAME_REQUIRED_if_name_not_specified_correctly(void)
{
    KineticLogInfo* pInfo;
    char nameData[] = "com.WD";
    ByteArray name;
    KineticStatus status;

    // Length is zero
    name.data = (uint8_t*)nameData;
    name.len = 0;
    KineticOperation_ValidateOperation_Expect(&Operation);
    status = KineticBuilder_BuildGetLog(&Operation,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE, name, &pInfo);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_NAME_REQUIRED, status);

    // Data is NULL
    name.data = NULL;
    name.len = strlen(nameData);
    KineticOperation_ValidateOperation_Expect(&Operation);
    status = KineticBuilder_BuildGetLog(&Operation,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE, name, &pInfo);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_NAME_REQUIRED, status);

    // Length is zero and data is NULL
    name.data = NULL;
    name.len = 0;
    KineticOperation_ValidateOperation_Expect(&Operation);
    status = KineticBuilder_BuildGetLog(&Operation,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__DEVICE, name, &pInfo);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_NAME_REQUIRED, status);
}

void test_KineticBuilder_BuildSetPin_should_build_a_SECURITY_operation_to_set_new_LOCK_PIN(void)
{
    char oldPinData[] = "1234";
    char newPinData[] = "5678";
    ByteArray oldPin = ByteArray_Create(oldPinData, sizeof(oldPinData));
    ByteArray newPin = ByteArray_Create(newPinData, sizeof(newPinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildSetPin(&Operation, oldPin, newPin, true);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutSetPin, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SECURITY,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.security, Request.command->body->security);
    TEST_ASSERT_TRUE(Request.command->body->security->has_oldlockpin);
    TEST_ASSERT_EQUAL_PTR(oldPinData, Request.command->body->security->oldlockpin.data);
    TEST_ASSERT_EQUAL(oldPin.len, Request.command->body->security->oldlockpin.len);
    TEST_ASSERT_TRUE(Request.command->body->security->has_newlockpin);
    TEST_ASSERT_EQUAL_PTR(newPinData, Request.command->body->security->newlockpin.data);
    TEST_ASSERT_EQUAL(newPin.len, Request.command->body->security->newlockpin.len);
    TEST_ASSERT_NULL(Request.command->body->pinop);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticBuilder_BuildSetPin_should_build_a_SECURITY_operation_to_set_new_ERASE_PIN(void)
{
    char oldPinData[] = "1234";
    char newPinData[] = "5678";
    ByteArray oldPin = ByteArray_Create(oldPinData, sizeof(oldPinData));
    ByteArray newPin = ByteArray_Create(newPinData, sizeof(newPinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildSetPin(&Operation, oldPin, newPin, false);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutSetPin, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SECURITY,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.security, Request.command->body->security);
    TEST_ASSERT_TRUE(Request.command->body->security->has_olderasepin);
    TEST_ASSERT_EQUAL_PTR(oldPinData, Request.command->body->security->olderasepin.data);
    TEST_ASSERT_EQUAL(oldPin.len, Request.command->body->security->olderasepin.len);
    TEST_ASSERT_TRUE(Request.command->body->security->has_newerasepin);
    TEST_ASSERT_EQUAL_PTR(newPinData, Request.command->body->security->newerasepin.data);
    TEST_ASSERT_EQUAL(newPin.len, Request.command->body->security->newerasepin.len);
    TEST_ASSERT_NULL(Request.command->body->pinop);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
}


void test_KineticBuilder_BuildErase_should_build_a_SECURE_ERASE_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildErase(&Operation, true, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinop);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinoptype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__SECURE_ERASE_PINOP,
        Request.command->body->pinop->pinoptype);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(180, Operation.timeoutSeconds);
}

void test_KineticBuilder_BuildErase_should_build_an_INSTANT_ERASE_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildErase(&Operation, false, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinop);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinoptype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__ERASE_PINOP,
        Request.command->body->pinop->pinoptype);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutErase, Operation.timeoutSeconds);
}

void test_KineticBuilder_BuildLockUnlock_should_build_a_LOCK_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildLockUnlock(&Operation, true, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinop);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinoptype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__LOCK_PINOP,
        Request.command->body->pinop->pinoptype);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutLockUnlock, Operation.timeoutSeconds);
}

void test_KineticBuilder_BuildLockUnlock_should_build_an_UNLOCK_operation_with_PIN_auth(void)
{
    char pinData[] = "abc123";
    ByteArray pin = ByteArray_Create(pinData, strlen(pinData));

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildLockUnlock(&Operation, false, &pin);

    TEST_ASSERT_NOT_NULL(Operation.pin);
    TEST_ASSERT_EQUAL_PTR(&pinData, Operation.pin->data);
    TEST_ASSERT_EQUAL_PTR(strlen(pinData), Operation.pin->len);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__PINOP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);
    TEST_ASSERT_EQUAL_PTR(&Request.message.pinOp, Request.command->body->pinop);
    TEST_ASSERT_TRUE(&Request.message.pinOp.has_pinoptype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__PIN_OPERATION__PIN_OP_TYPE__UNLOCK_PINOP,
        Request.command->body->pinop->pinoptype);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_Basic, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutLockUnlock, Operation.timeoutSeconds);
}

void test_KineticBuilder_BuildSetClusterVersion_should_build_a_SET_CLUSTER_VERSION_operation_with_PIN_auth(void)
{
    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildSetClusterVersion(&Operation, 1776);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SETUP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);

    TEST_ASSERT_EQUAL_PTR(&Request.message.setup, Request.command->body->setup);
    TEST_ASSERT_EQUAL_INT64(1776, Request.message.setup.newclusterversion);
    TEST_ASSERT_EQUAL_INT64(1776, Operation.pendingClusterVersion);
    TEST_ASSERT_TRUE(Request.message.setup.has_newclusterversion);
    TEST_ASSERT_FALSE(Request.message.setup.has_firmwaredownload);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_SetClusterVersion, Operation.opCallback);
}

void test_KineticBuilder_BuildSetACL_should_build_a_SECURITY_operation(void)
{
    struct ACL ACLs = {
        .ACL_count = 1,
        .ACL_ceil = 1,
        .ACLs = NULL,
    };

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticBuilder_BuildSetACL(&Operation, &ACLs);

    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SECURITY,
        Request.message.command.header->messagetype);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);

    TEST_ASSERT_FALSE(Request.command->body->security->has_oldlockpin);
    TEST_ASSERT_FALSE(Request.command->body->security->has_newlockpin);
    TEST_ASSERT_FALSE(Request.command->body->security->has_olderasepin);
    TEST_ASSERT_FALSE(Request.command->body->security->has_newerasepin);

    TEST_ASSERT_EQUAL_PTR(Request.command->body->security->acl, ACLs.ACLs);

    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_SetACL, Operation.opCallback);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(KineticOperation_TimeoutSetACL, Operation.timeoutSeconds);
}

void test_KineticBuilder_BuildUpdateFirmware_should_build_a_FIRMWARE_DOWNLOAD_operation(void)
{
    const char* path = "test/support/data/dummy_fw.slod";
    const char* fwBytes = "firmware\nupdate\ncontents\n";
    int fwLen = strlen(fwBytes);

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticStatus status = KineticBuilder_BuildUpdateFirmware(&Operation, path);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_FALSE(Request.pinAuth);
    TEST_ASSERT_NOT_NULL(Operation.value.data);
    TEST_ASSERT_EQUAL(fwLen, Operation.value.len);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL(0, Operation.timeoutSeconds);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messagetype);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__MESSAGE_TYPE__SETUP,
        Request.message.command.header->messagetype);
    TEST_ASSERT_EQUAL_PTR(&Request.message.body, Request.command->body);

    TEST_ASSERT_EQUAL_PTR(&Request.message.setup, Request.command->body->setup);
    TEST_ASSERT_TRUE(Request.message.setup.firmwaredownload);
    TEST_ASSERT_TRUE(Request.message.setup.has_firmwaredownload);
    TEST_ASSERT_EQUAL_PTR(&KineticCallbacks_UpdateFirmware, Operation.opCallback);

    TEST_ASSERT_NOT_NULL(Operation.value.data);
    TEST_ASSERT_EQUAL(fwLen, Operation.value.len);
    TEST_ASSERT_EQUAL_STRING(fwBytes, (char*)Operation.value.data);
}

void test_KineticBuilder_BuildUpdateFirmware_should_return_INVALID_FILE_if_does_not_exist(void)
{
    const char* path = "test/support/data/none.slod";

    KineticOperation_ValidateOperation_Expect(&Operation);

    KineticStatus status = KineticBuilder_BuildUpdateFirmware(&Operation, path);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_FILE, status);
}
