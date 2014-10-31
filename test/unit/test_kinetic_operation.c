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
#include "mock_kinetic_types_internal.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"

static KineticConnection Connection;
static int64_t ConnectionID = 12345;
static KineticPDU Request, Response;
static KineticPDU Requests[3];
static KineticOperation Operation;


void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connectionID = ConnectionID;
    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    KINETIC_PDU_INIT_WITH_COMMAND(&Response, &Connection);
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.request = &Request;
}

void tearDown(void)
{
    KineticLogger_Close();
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


void test_KineticOperation_SendRequest_should_transmit_PDU_with_no_value_payload(void)
{
    LOG_LOCATION;
    KineticProto_Message* msg = &Request.protoData.message.message;

    // KineticProto_Message__init(msg);
    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    ByteBuffer headerNBO = ByteBuffer_Create(&Request.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    // KineticEntry entry = {.value = BYTE_BUFFER_NONE};
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.request = &Request;

    uint8_t packedCommandBytes[1024];

    // Pack message `command` element in order to precalculate fully packed message size 
    size_t expectedCommandLen = KineticProto_command__get_packed_size(&Request.protoData.message.command);
    Request.protoData.message.message.commandBytes.data = packedCommandBytes;
    assert(Request.protoData.message.message.commandBytes.data != NULL);
    size_t packedCommandLen = KineticProto_command__pack(
        &Request.protoData.message.command,
        Request.protoData.message.message.commandBytes.data);
    assert(packedCommandLen == expectedCommandLen);
    Request.protoData.message.message.commandBytes.len = packedCommandLen;
    Request.protoData.message.message.has_commandBytes = true;

    // Create NBO copy of header for sending
    Request.header.versionPrefix = 'F';
    Request.header.protobufLength = KineticProto_Message__get_packed_size(msg);
    Request.header.valueLength = 0;
    Request.headerNBO.versionPrefix = Request.header.versionPrefix;
    Request.headerNBO.protobufLength = KineticNBO_FromHostU32(Request.header.protobufLength);
    Request.headerNBO.valueLength = KineticNBO_FromHostU32(Request.header.valueLength);

    // Setup expectations for interaction
    KineticHMAC_Init_Expect(&Request.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac, &Request.protoData.message.message, Request.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &Request, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticOperation_SendRequest(&Operation);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticOperation_SendRequest_should_send_PDU_with_value_payload(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&Request.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    uint8_t valueData[128];
    ByteBuffer valueBuffer = ByteBuffer_Create(valueData, sizeof(valueData), 0);
    ByteBuffer_AppendCString(&valueBuffer, "Some arbitrary value");
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.entry.value = valueBuffer;
    Operation.request = &Request;
    Operation.entryEnabled = true;
    Operation.valueEnabled = true;
    Operation.sendValue = true;

    KineticHMAC_Init_Expect(&Request.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac,
        &Request.protoData.message.message, Request.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &Request, KINETIC_STATUS_SUCCESS);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &Operation.entry.value, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticOperation_SendRequest(&Operation);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticOperation_SendRequest_should_send_the_specified_message_and_return_false_upon_failure_to_send_header(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&Request.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    char valueData[] = "Some arbitrary value";
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.entry.value = ByteBuffer_Create(valueData, strlen(valueData), strlen(valueData));
    Operation.request = &Request;
    Operation.entryEnabled = true;
    Operation.valueEnabled = true;
    Operation.sendValue = true;

    KineticHMAC_Init_Expect(&Request.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac, &Request.protoData.message.message, Request.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SOCKET_ERROR);

    KineticStatus status = KineticOperation_SendRequest(&Operation);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_ERROR, status);
}

void test_KineticOperation_SendRequest_should_send_the_specified_message_and_return_false_upon_failure_to_send_protobuf(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&Request.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    char valueData[] = "Some arbitrary value";
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.entry.value = ByteBuffer_Create(valueData, strlen(valueData), strlen(valueData));
    Operation.request = &Request;
    Operation.entryEnabled = true;
    Operation.valueEnabled = true;
    Operation.sendValue = true;

    KineticHMAC_Init_Expect(&Request.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac,
        &Request.protoData.message.message, Request.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &Request, KINETIC_STATUS_SOCKET_TIMEOUT);

    KineticStatus status = KineticOperation_SendRequest(&Operation);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_TIMEOUT, status);
}

void test_KineticOperation_SendRequest_should_send_the_specified_message_and_return_KineticStatus_if_value_write_fails(void)
{
    LOG_LOCATION;
    ByteBuffer headerNBO = ByteBuffer_Create(&Request.headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));

    KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    uint8_t valueData[128];
    KINETIC_OPERATION_INIT(&Operation, &Connection);
    Operation.entry.value = ByteBuffer_Create(valueData, sizeof(valueData), 0);
    ByteBuffer_AppendCString(&Operation.entry.value, "Some arbitrary value");
    Operation.request = &Request;
    Operation.entryEnabled = true;
    Operation.valueEnabled = true;
    Operation.sendValue = true;

    KineticHMAC_Init_Expect(&Request.hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&Request.hmac, &Request.protoData.message.message, Request.connection->session.hmacKey);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &headerNBO, KINETIC_STATUS_SUCCESS);
    KineticSocket_WriteProtobuf_ExpectAndReturn(Connection.socket, &Request, KINETIC_STATUS_SUCCESS);
    KineticSocket_Write_ExpectAndReturn(Connection.socket, &Operation.entry.value, KINETIC_STATUS_SOCKET_TIMEOUT);

    KineticStatus status = KineticOperation_SendRequest(&Operation);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SOCKET_TIMEOUT, status);
}



void test_KineticOperation_GetStatus_should_return_KINETIC_STATUS_INVALID_if_no_KineticProto_Command_Status_StatusCode_in_response(void)
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



void test_KineticOperation_AssociateResponseWithOperation_should_return_NULL_if_supplied_PDU_is_invalid(void)
{
    LOG_LOCATION;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(NULL));

    Response.type = KINETIC_PDU_TYPE_RESPONSE;
    TEST_ASSERT_NOT_NULL(Response.command);
    TEST_ASSERT_NOT_NULL(Response.command->header);
    TEST_ASSERT_FALSE(Response.command->header->has_ackSequence);

    Response.type = KINETIC_PDU_TYPE_REQUEST;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));

    Response.type = KINETIC_PDU_TYPE_UNSOLICITED;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));

    Response.type = KINETIC_PDU_TYPE_RESPONSE;
    Response.command->header->has_ackSequence = false;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));

    Response.command->header = NULL;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));

    Response.command = NULL;
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));
}

void test_KineticOperation_AssociateResponseWithOperation_should_return_NULL_if_no_matching_request_was_found_in_PDU_list(void)
{
    LOG_LOCATION;

    Response.type = KINETIC_PDU_TYPE_RESPONSE;
    Response.command->header->has_ackSequence = true;
    Response.command->header->ackSequence = 9876543210;
    TEST_ASSERT_EQUAL_PTR(&Connection, Response.connection);
    TEST_ASSERT_NOT_NULL(Response.command);
    TEST_ASSERT_NOT_NULL(Response.command->header);

    KineticOperation ops[3];
    for(int i = 0; i < 3; i++) {
        KINETIC_PDU_INIT_WITH_COMMAND(&Requests[i], &Connection);
        Requests[i].command->header->has_sequence = true;
        Requests[i].type = KINETIC_PDU_TYPE_REQUEST;
        KINETIC_OPERATION_INIT(&ops[i], &Connection);
    }

    // Empty operations list should result in NULL being returned (no match)
    KineticAllocator_GetFirstOperation_ExpectAndReturn(&Connection, NULL);
    TEST_ASSERT_NULL(KineticOperation_AssociateResponseWithOperation(&Response));

    // A list with only the expected operation w/ matching request PDU
    Requests[0].command->header->sequence = 9876543210;
    ops[0].request = &Requests[0];
    KineticAllocator_GetFirstOperation_ExpectAndReturn(&Connection, &ops[0]);
    TEST_ASSERT_EQUAL_PTR(&ops[0], KineticOperation_AssociateResponseWithOperation(&Response));
    TEST_ASSERT_EQUAL_PTR(&Response, ops[0].response);

    // A list starting with a non-matching PDU, followed by the expected PDU
    Requests[0].command->header->sequence = 12345;
    ops[0].request = &Requests[0];
    ops[0].response = NULL;
    Requests[1].command->header->sequence = 9876543210;
    ops[1].request = &Requests[1];
    ops[1].response = NULL;
    KineticAllocator_GetFirstOperation_ExpectAndReturn(&Connection, &ops[0]);
    KineticAllocator_GetNextOperation_ExpectAndReturn(&Connection, &ops[0], &ops[1]);
    TEST_ASSERT_EQUAL_PTR(&ops[1], KineticOperation_AssociateResponseWithOperation(&Response));
    TEST_ASSERT_EQUAL_PTR(&Response, ops[1].response);
    TEST_ASSERT_NULL(ops[0].response);

    // A list starting with with multiple non-matching PDUs, followed by the expected PDU
    Requests[0].command->header->sequence = 12345;
    ops[0].request = &Requests[0];
    ops[0].response = NULL;
    Requests[1].command->header->sequence = 45678;
    ops[1].request = &Requests[1];
    ops[1].response = NULL;
    Requests[2].command->header->sequence = 9876543210;
    ops[2].request = &Requests[2];
    ops[2].response = NULL;
    KineticAllocator_GetFirstOperation_ExpectAndReturn(&Connection, &ops[0]);
    KineticAllocator_GetNextOperation_ExpectAndReturn(&Connection, &ops[0], &ops[1]);
    KineticAllocator_GetNextOperation_ExpectAndReturn(&Connection, &ops[1], &ops[2]);
    TEST_ASSERT_EQUAL_PTR(&ops[2], KineticOperation_AssociateResponseWithOperation(&Response));
    TEST_ASSERT_EQUAL_PTR(&Response, ops[2].response);
    TEST_ASSERT_NULL(ops[1].response);
    TEST_ASSERT_NULL(ops[0].response);
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
    TEST_ASSERT_TRUE(Request.protoData.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP, Request.protoData.message.command.header->messageType);
    TEST_ASSERT_ByteBuffer_NULL(Operation.entry.value);
    TEST_ASSERT_NULL(Operation.response);
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
    TEST_ASSERT_TRUE(Operation.entryEnabled);
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_TRUE(Operation.sendValue);
    TEST_ASSERT_TRUE(Request.protoData.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT, Request.protoData.message.command.header->messageType);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry.value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry.value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
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
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

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
    TEST_ASSERT_TRUE(Request.protoData.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.protoData.message.command.header->messageType);
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

    TEST_ASSERT_TRUE(Operation.entryEnabled);
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry.value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry.value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry.value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry.metadataOnly);
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
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0 for a metadata-only request

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
    TEST_ASSERT_TRUE(Request.protoData.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.protoData.message.command.header->messageType);
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

    TEST_ASSERT_TRUE(Operation.entryEnabled);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry.value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry.value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry.value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry.metadataOnly);
}


void test_KineticOperation_BuildDelete_should_build_a_DELETE_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {.key = ByteBuffer_CreateWithArray(key), .value = ByteBuffer_CreateWithArray(value)};

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
    TEST_ASSERT_TRUE(Request.protoData.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE, Request.protoData.message.command.header->messageType);
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

    TEST_ASSERT_TRUE(Operation.entryEnabled);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry.value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry.value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry.value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
}


void test_KineticOperation_BuildGetKeyRange_should_build_a_GetKeyRange_request(void)
{
    LOG_LOCATION;

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

    KineticOperation_BuildGetKeyRange(&Operation, &range, keys);

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
    TEST_ASSERT_TRUE(Request.command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE, Request.command->header->messageType);
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

    TEST_ASSERT_FALSE(Operation.entryEnabled);
    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.value);
    TEST_ASSERT_EQUAL_PTR(&Request, Operation.request);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.key);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.tag);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.dbVersion);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.newVersion);
    TEST_ASSERT_ByteBuffer_EMPTY(Operation.entry.value);
    TEST_ASSERT_EQUAL_PTR(&Request.protoData.message, Request.proto);
    TEST_ASSERT_EQUAL_PTR(&Request.protoData.message.command, Request.command);
}
