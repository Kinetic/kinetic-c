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
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_types_internal.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"

static KineticConnection Connection;
static int64_t ConnectionID = 12345;
static ByteArray HMACKey;
static KineticPDU Request, Response;
static KineticOperation Operation;

void setUp(void)
{
    HMACKey = ByteArray_CreateWithCString("some_hmac_key");
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connectionID = ConnectionID;
    KINETIC_PDU_INIT_WITH_MESSAGE(&Request, &Connection);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Response, &Connection);
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.request = &Request;
    Operation.response = &Response;
}

void tearDown(void)
{
}

void test_KINETIC_OPERATION_INIT_should_configure_the_operation(void)
{
    LOG_LOCATION;
    KineticOperation op = {
        .connection = NULL,
        .request = NULL,
        .response = NULL,
    };

    KINETIC_OPERATION_INIT(&op, &Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, op.connection);
    TEST_ASSERT_NULL(op.request);
    TEST_ASSERT_NULL(op.response);
}


void test_KineticOperation_Create_should_create_a_new_operation_with_allocated_PDUs(void)
{
    LOG_LOCATION;
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);

    KineticOperation operation = KineticOperation_Create(&Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, operation.connection);
    TEST_ASSERT_EQUAL_INT64(ConnectionID, Connection.connectionID);
    TEST_ASSERT_EQUAL_INT64(ConnectionID, operation.request->proto->command->header->connectionID);
    TEST_ASSERT_NOT_NULL(operation.request);
    TEST_ASSERT_NOT_NULL(operation.response);
}

void test_KineticOperation_Free_should_free_an_operation_with_allocated_PDUs(void)
{
    LOG_LOCATION;
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);

    KineticOperation operation = KineticOperation_Create(&Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, operation.connection);
    TEST_ASSERT_NOT_NULL(operation.request);
    TEST_ASSERT_NOT_NULL(operation.response);

    KineticAllocator_FreePDU_Expect(&Connection.pdus, operation.request);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, operation.response);
    KineticStatus status = KineticOperation_Free(&operation);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticOperation_GetStatus_should_return_KINETIC_STATUS_INVALID_if_no_KineticProto_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    status = KineticOperation_GetStatus(NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Operation.response = NULL;
    KineticPDU_GetStatus_ExpectAndReturn(NULL, KINETIC_STATUS_INVALID);
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    // Build a valid NOOP to facilitate testing protobuf structure and status extraction
    Operation.request = &Request;
    Operation.response = &Response;
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticOperation_BuildNoop(&Operation);

    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_CONNECTION_ERROR);
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    LOG_LOCATION;

    KineticConnection_IncrementSequence_Expect(&Connection);

    KineticOperation_BuildNoop(&Operation);

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
    //
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_NOOP, Request.proto->command->header->messageType);
    TEST_ASSERT_ByteBuffer_NULL(Request.entry.value);
    TEST_ASSERT_ByteBuffer_NULL(Response.entry.value);
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object(void)
{
    LOG_LOCATION;
    ByteArray value = ByteArray_CreateWithCString("Luke, I am your father");
    ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray newVersion = ByteArray_CreateWithCString("v1.0");
    ByteArray tag = ByteArray_CreateWithCString("some_tag");

    KineticConnection_IncrementSequence_Expect(&Connection);

    // PUT
    // The PUT operation sets the value and metadata for a given key. If a value
    // already exists in the store for the given key, the client must pass a
    // value for dbVersion which matches the stored version for this key to
    // overwrite the value metadata. This behavior can be overridden (so that
    // the version is ignored and the value and metadata are always written) by
    // setting forced to true in the KeyValue option.
    //
    // Request Message:
    //
    // command {
    //   // See top level cross cutting concerns for header details
    //   header {
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //     messageType: PUT
    //   }
    //   body: {
    //     keyValue {
    //       // Required bytes
    //       // The key for the value being set
    //       key: "..."
    //
    //       // Required bytes
    //       // Versions are set on objects to support optimistic locking.
    //       // For operations that modify data, if the dbVersion sent in the
    //       // request message does not match the version stored in the db, the
    //       // request will fail.
    //       dbVersion: "..."
    //
    //       // Required bytes
    //       // Specifies what the next version of the data will be if this
    //       // operation is successful.
    //       newVersion: "..."
    //
    //       // Optional bool, default false
    //       // Setting force to true ignores potential version mismatches
    //       // and carries out the operation.
    //       force: true
    //
    //       // Optional bytes
    //       // The integrity value for the data. This value should be computed
    //       // by the client application by applying the hash algorithm
    //       // specified below to the value (and only to the value).
    //       // The algorithm used should be specified in the algorithm field.
    //       // The Kinetic Device will not do any processing on this value.
    //       tag: "..."
    //
    //       // Optional enum
    //       // The algorithm used by the client to compute the tag.
    //       // The allowed values are: SHA1, SHA2, SHA3, CRC32, CRC64
    //       algorithm: ...
    //
    //       // Optional Synchronization enum value, defaults to WRITETHROUGH
    //       // Allows client to specify if the data must be written to disk
    //       // immediately, or can be written in the future.
    //       //
    //       // WRITETHROUGH:  This request is made persistent before returning.
    //       //                This does not effect any other pending operations.
    //       // WRITEBACK:     They can be made persistent when the drive chooses,
    //       //            or when a subsequent FLUSH is give to the drive.
    //       // FLUSH:     All pending information that has not been written is
    //       //        pushed to the disk and the command that specifies
    //       //        FLUSH is written last and then returned. All WRITEBACK writes
    //       //        that have received ending status will be guaranteed to be
    //       //        written before the FLUSH operation is returned completed.
    //       synchronization: ...
    //     }
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .newVersion = ByteBuffer_CreateWithArray(newVersion),
        // .dbVersion = ByteBuffer_CreateWithArray(BYTE_ARRAY_NONE),
        .tag = ByteBuffer_CreateWithArray(tag),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateWithArray(value),
    };
    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->protoData.message, &entry);
    //   }
    // }
    // hmac: "..."
    //

    // Build the operation
    KineticOperation_BuildPut(&Operation, &entry);

    // Ensure proper message type
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_PUT, Request.proto->command->header->messageType);

    TEST_ASSERT_EQUAL_ByteArray(value, Operation.request->entry.value.array);
    TEST_ASSERT_EQUAL(0, Operation.request->entry.value.bytesUsed);
    TEST_ASSERT_ByteBuffer_NULL(Response.entry.value);
}

uint8_t ValueData[KINETIC_OBJ_SIZE];

void test_KineticOperation_BuildGet_should_build_a_GET_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &entry);

    KineticOperation_BuildGet(&Operation, &entry);

    // GET
    // The GET operation is used to retrieve the value and metadata for a given key.
    //
    // Request Message:
    // command {
    //   header {
    //     // See above for descriptions of these fields
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //
    //     // The mesageType should be GET
    //     messageType: GET
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_GET, Request.proto->command->header->messageType);
    //   }
    //   body {
    //     keyValue {
    //       // See above
    //       key: "..."
    //     }
    //   }
    // }
    // // See above
    // hmac: "..."

    TEST_ASSERT_ByteBuffer_NULL(Request.entry.value);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.response->entry.value.array);
    TEST_ASSERT_EQUAL(0, Operation.response->entry.value.bytesUsed);
}

void test_KineticOperation_BuildGet_should_build_a_GET_operation_requesting_metadata_only(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .metadataOnly = true,
        .value = ByteBuffer_CreateWithArray(value),
    };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &entry);

    KineticOperation_BuildGet(&Operation, &entry);

    // GET
    // The GET operation is used to retrieve the value and metadata for a given key.
    //
    // Request Message:
    // command {
    //   header {
    //     // See above for descriptions of these fields
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //
    //     // The mesageType should be GET
    //     messageType: GET
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_GET, Request.proto->command->header->messageType);
    //   }
    //   body {
    //     keyValue {
    //       // See above
    //       key: "..."
    //     }
    //   }
    // }
    // // See above
    // hmac: "..."

    TEST_ASSERT_ByteBuffer_NULL(Request.entry.value);
    TEST_ASSERT_ByteBuffer_NULL(Response.entry.value);
}


void test_KineticOperation_BuildDelete_should_build_a_DELETE_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    KineticEntry entry = {.key = ByteBuffer_CreateWithArray(key)};

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &entry);

    KineticOperation_BuildDelete(&Operation, &entry);

    // The `DELETE` operation removes the entry for a given key. It respects the
    // same locking behavior around `dbVersion` and `force` as described in the previous sections.
    // The following request will remove a key value pair to the store.
    //
    // ```
    // command {
    //   // See top level cross cutting concerns for header details
    //   header {
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //     // messageType should be DELETE
    //     messageType: DELETE
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_DELETE, Request.proto->command->header->messageType);
    //   }
    //   body {
    //     keyValue {
    //       key: "..."
    //       // See write operation cross cutting concerns
    //       synchronization: ...
    //     }
    //   }
    // }
    // hmac: "..."

    TEST_ASSERT_ByteBuffer_NULL(Request.entry.value);
    TEST_ASSERT_ByteBuffer_NULL(Response.entry.value);
}


void test_KineticOperation_BuildGetKeyRange_should_build_a_GetKeyRange_request(void)
{
    LOG_LOCATION;

    const int numKeysInRange = 4;
    uint8_t startKeyData[32];
    uint8_t endKeyData[32];
    ByteBuffer startKey, endKey;

    startKey = ByteBuffer_Create(startKeyData, sizeof(startKeyData));
    ByteBuffer_AppendCString(&startKey, "key_range_00_00");
    endKey = ByteBuffer_Create(endKeyData, sizeof(endKeyData));
    ByteBuffer_AppendCString(&endKey, "key_range_00_03");

    KineticKeyRange range = {
        .startKey = startKey,
        .endKey = endKey,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = numKeysInRange,
        .reverse = false,
    };

    // KineticProto_Range protoKeyRangeRequest = {
    //     .has_startKey = true,
    //     .startKey = (ProtobufCBinaryData) {
    //         .data = StartKey.array.data,
    //         .len = StartKey.bytesUsed},
    //     .has_endKey = true,
    //     .endKey = (ProtobufCBinaryData) {
    //         .data = StartKey.array.data,
    //         .len = StartKey.bytesUsed},
    //     .has_startKeyInclusive = true,
    //     .startKeyInclusive = true,
    //     .has_endKeyInclusive = true,
    //     .endKeyInclusive = true,
    //     .has_maxReturned = true,
    //     .maxReturned = NUM_KEYS_IN_RANGE,
    // };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyRange_Expect(&Request.protoData.message, &range);

    KineticOperation_BuildGetKeyRange(&Operation, &range);

    // The `DELETE` operation removes the entry for a given key. It respects the
    // same locking behavior around `dbVersion` and `force` as described in the previous sections.
    // The following request will remove a key value pair to the store.
    //
    // ```
    // command {
    //   // See top level cross cutting concerns for header details
    //   header {
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //     // messageType should be DELETE
    //     messageType: DELETE
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_GETKEYRANGE, Request.proto->command->header->messageType);
    //   }
    //   body {
    //     keyValue {
    //       key: "..."
    //       // See write operation cross cutting concerns
    //       synchronization: ...
    //     }
    //   }
    // }
    // hmac: "..."

    TEST_ASSERT_ByteBuffer_NULL(Request.entry.value);
    TEST_ASSERT_ByteBuffer_NULL(Response.entry.value);

    TEST_ASSERT_EQUAL_PTR(&Request.protoData.message, Request.proto);
    // TEST_ASSERT_EQUAL_PTR(&Request.protoData.message.command, Request.proto->command);
    // TEST_ASSERT_EQUAL_PTR(&Request.protoData.message.body, Request.proto->command->body);
    // TEST_ASSERT_EQUAL_PTR(&Request.protoData.message.keyRange, Request.proto->command->body->range);
    // TEST_ASSERT_EQUAL_PTR(&Request.protoData.message.keyRange, Request.proto->command->body->range);
}
