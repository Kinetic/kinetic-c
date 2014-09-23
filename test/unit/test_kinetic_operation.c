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
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
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
    HMACKey = BYTE_ARRAY_INIT_FROM_CSTRING("some_hmac_key");
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
    KineticAllocator_NewPDU_ExpectAndReturn(&Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Response);
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
    KineticAllocator_NewPDU_ExpectAndReturn(&Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);

    KineticOperation operation = KineticOperation_Create(&Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, operation.connection);
    TEST_ASSERT_NOT_NULL(operation.request);
    TEST_ASSERT_NOT_NULL(operation.response);

    KineticAllocator_FreePDU_Expect(&operation.request);
    KineticAllocator_FreePDU_Expect(&operation.response);
    KineticStatus status = KineticOperation_Free(&operation);
    TEST_ASSERT_EQUAL_STATUS(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticOperation_GetStatus_should_return_KINETIC_STATUS_INVALID_if_no_KineticProto_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    status = KineticOperation_GetStatus(NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Operation.response = NULL;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    // Build a valid NOOP to facilitate testing protobuf structure and status extraction
    Operation.request = &Request;
    Operation.response = &Response;
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticOperation_BuildNoop(&Operation);

    KineticProto* proto = Response.proto;
    Response.proto = NULL;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
    Response.proto = proto;

    KineticProto_Command* cmd = Response.proto->command;
    TEST_ASSERT_NOT_NULL(cmd);
    Response.proto->command = NULL;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
    Response.proto->command = cmd;

    Response.proto->command->status = NULL;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Response.proto->command->status = &Response.protoData.message.status;
    Response.proto->command->status->has_code = false;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Response.proto->command->status->has_code = true;
    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
}

void test_KineticOperation_GetStatus_should_return_appropriate_KineticStatus_based_on_KineticProto_Status_StatusCode_in_response(void)
{
    LOG_LOCATION;
    KineticStatus status;

    // Build a valid NOOP to facilitate testing protobuf structure and status extraction
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticOperation_BuildNoop(&Operation);

    Response.proto = &Response.protoData.message.proto;
    Response.proto->command = &Response.protoData.message.command;
    Response.proto->command->status = &Response.protoData.message.status;
    Response.proto->command->status->has_code = true;

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DEVICE_BUSY, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_REQUEST;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_HEADER_REQUIRED;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DATA_ERROR, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DATA_ERROR, status);
    
    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DATA_ERROR, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_HMAC_FAILURE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DATA_ERROR, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NOT_FOUND;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_DATA_ERROR, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_VERSION_FAILURE, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_FAILURE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_VERSION_FAILURE, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_INTERNAL_ERROR;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_OPERATION_FAILED, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NOT_AUTHORIZED;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_OPERATION_FAILED, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_EXPIRED;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_OPERATION_FAILED, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NO_SPACE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_OPERATION_FAILED, status);

    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_OPERATION_FAILED, status);


    Response.proto->command->status->code = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Response.proto->command->status->code = _KINETIC_PROTO_STATUS_STATUS_CODE_IS_INT_SIZE;
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);

    Response.proto->command->status->code = (KineticProto_Status_StatusCode)
        (KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS + 10);
    status = KineticOperation_GetStatus(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID, status);
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
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object(void)
{
    LOG_LOCATION;
    ByteArray value = BYTE_ARRAY_INIT_FROM_CSTRING("Luke, I am your father");

    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("foobar");
    const ByteArray newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    const ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("some_tag");

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
    const KineticKeyValue metadata = {
        .key = key,
        .newVersion = newVersion,
        .dbVersion = BYTE_ARRAY_NONE,
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = value,
    };
    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->protoData.message, &metadata);
    //   }
    // }
    // hmac: "..."
    //

    // Build the operation
    KineticOperation_BuildPut(&Operation, &metadata);

    // Ensure proper message type
    TEST_ASSERT_TRUE(Request.proto->command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_PUT, Request.proto->command->header->messageType);

    TEST_ASSERT_EQUAL_ByteArray(value, Operation.request->value);
}

uint8_t ValueData[PDU_VALUE_MAX_LEN];

void test_KineticOperation_BuildGet_should_build_a_GET_operation_with_supplied_value_ByteArray(void)
{
    LOG_LOCATION;
    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("foobar");
    ByteArray value = {.data = ValueData};
    ByteArray expectedValue = {.data = value.data, .len = PDU_VALUE_MAX_LEN};
    const KineticKeyValue metadata = {.key = key, .value = (ByteArray){.len = PDU_VALUE_MAX_LEN, .data = value.data}};

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &metadata);

    KineticOperation_BuildGet(&Operation, &metadata);

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

    TEST_ASSERT_EQUAL_ByteArray(expectedValue, Response.value);
}

void test_KineticOperation_BuildGet_should_build_a_GET_operation_with_embedded_value_ByteArray(void)
{
    LOG_LOCATION;
    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("foobar");
    const KineticKeyValue metadata = {
        .key = key,
    };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &metadata);

    KineticOperation_BuildGet(&Operation, &metadata);

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

    TEST_ASSERT_ByteArray_NONE(Request.value);
    TEST_ASSERT_EQUAL_PTR(Response.valueBuffer, Response.value.data);
}

void test_KineticOperation_BuildGet_should_build_a_GET_operation_requesting_metadata_only(void)
{
    LOG_LOCATION;
    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("foobar");
    const KineticKeyValue metadata = {
        .key = key,
        .metadataOnly = true,
    };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &metadata);

    KineticOperation_BuildGet(&Operation, &metadata);

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

    TEST_ASSERT_ByteArray_NONE(Request.value);
}


void test_KineticOperation_BuildDelete_should_build_a_DELETE_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("foobar");
    const KineticKeyValue metadata = {
        .key = key,
    };

    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &metadata);

    KineticOperation_BuildDelete(&Operation, &metadata);

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

    TEST_ASSERT_ByteArray_NONE(Request.value);
    TEST_ASSERT_ByteArray_NONE(Response.value);
}
