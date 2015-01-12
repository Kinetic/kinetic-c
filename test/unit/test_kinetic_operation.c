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
    Connection.connectionID = ConnectionID;
    Session.connection = &Connection;
    KineticPDU_InitWithCommand(&Request, &Connection);
    KineticPDU_InitWithCommand(&Response, &Connection);
    KineticOperation_Init(&Operation, &Connection);
    Operation.request = &Request;
    Operation.connection = &Connection;
    SessionConfig = (KineticSessionConfig) {.host = "anyhost", .port = KINETIC_PORT};
    Session = (KineticSession) {.config = SessionConfig, .connection = &Connection};
    Connection.session = Session;
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticOperation_Init_should_configure_the_operation(void)
{
    LOG_LOCATION;
    KineticOperation op = {
        .connection = NULL,
        .request = NULL,
        .response = NULL,
    };

    KineticOperation_Init(&op, &Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, op.connection);
    TEST_ASSERT_NULL(op.request);
    TEST_ASSERT_NULL(op.response);
}


void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    LOG_LOCATION;

    KineticSession_IncrementSequence_Expect(&Session);

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

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_NOOP, Request.message.command.header->messageType);
    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation_to_create_a_new_object(void)
{
    LOG_LOCATION;
    ByteArray value = ByteArray_CreateWithCString("Luke, I am your father");
    ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray newVersion = ByteArray_CreateWithCString("v1.0");
    ByteArray tag = ByteArray_CreateWithCString("some_tag");

    KineticSession_IncrementSequence_Expect(&Session);

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
    KineticMessage_ConfigureKeyValue_Expect(&Operation.request->message, &entry);
    //   }
    // }
    // hmac: "..."
    //

    // Build the operation
    KineticOperation_BuildPut(&Operation, &entry);

    // Ensure proper message type
    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_TRUE(Operation.sendValue);
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_PUT,
        Request.message.command.header->messageType);
    TEST_ASSERT_EQUAL_ByteArray(value, Operation.entry->value.array);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
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

    KineticSession_IncrementSequence_Expect(&Session);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

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
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.message.command.header->messageType);
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

    TEST_ASSERT_TRUE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
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

    KineticSession_IncrementSequence_Expect(&Session);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

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
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GET, Request.message.command.header->messageType);
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

    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetNext_should_build_a_GETNEXT_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

    KineticSession_IncrementSequence_Expect(&Session);
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
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetNext_should_build_a_GETNEXT_operation_with_metadata_only(void)
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

    KineticSession_IncrementSequence_Expect(&Session);
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
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetPrevious_should_build_a_GETPREVIOUS_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = {.data = ValueData, .len = sizeof(ValueData)};
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .value = ByteBuffer_CreateWithArray(value),
    };
    entry.value.bytesUsed = 123; // Set to non-empty state, since it should be reset to 0

    KineticSession_IncrementSequence_Expect(&Session);
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
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_FALSE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildGetPrevious_should_build_a_GETPREVIOUS_operation_with_metadata_only(void)
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

    KineticSession_IncrementSequence_Expect(&Session);
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
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_TRUE(Operation.entry->metadataOnly);
}

void test_KineticOperation_BuildFlush_should_build_a_FLUSHALLDATA_operation(void)
{
    KineticSession_IncrementSequence_Expect(&Session);

    KineticOperation_BuildFlush(&Operation);

    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_FLUSHALLDATA,
        Request.message.command.header->messageType);

    TEST_ASSERT_NULL(Operation.response);
}

void test_KineticOperation_BuildDelete_should_build_a_DELETE_operation(void)
{
    LOG_LOCATION;
    const ByteArray key = ByteArray_CreateWithCString("foobar");
    ByteArray value = ByteArray_Create(ValueData, sizeof(ValueData));
    KineticEntry entry = {.key = ByteBuffer_CreateWithArray(key), .value = ByteBuffer_CreateWithArray(value)};

    KineticSession_IncrementSequence_Expect(&Session);
    KineticMessage_ConfigureKeyValue_Expect(&Request.message, &entry);

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
    TEST_ASSERT_TRUE(Request.message.command.header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_DELETE, Request.message.command.header->messageType);
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

    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_EQUAL_PTR(value.data, Operation.entry->value.array.data);
    TEST_ASSERT_EQUAL_PTR(value.len, Operation.entry->value.array.len);
    TEST_ASSERT_EQUAL(0, Operation.entry->value.bytesUsed);
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

    KineticSession_IncrementSequence_Expect(&Session);
    KineticMessage_ConfigureKeyRange_Expect(&Request.message, &range);

    KineticOperation_BuildGetKeyRange(&Operation, &range, &keys);

    // Get Key Range
    // 
    // The GETKEYRANGE operation takes a start and end key and returns all keys between those in the sorted set of keys.
    // This operation can be configured so that the range is either inclusive or exclusive of the start and end keys,
    // the range can be reversed, and the requester can cap the number of keys returned.
    // 
    // Note that this operation does not fetch associated values, or other metadata. It only returns the keys themselves,
    // which can be used for other operations.
    // 
    // Request Message
    //
    // command {
    //   header {
    //     // See above for descriptions of these fields
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //
    //     // messageType should be GETKEYRANGE
    //     messageType: GETKEYRANGE
    //   }
    TEST_ASSERT_TRUE(Request.command->header->has_messageType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_MESSAGE_TYPE_GETKEYRANGE, Request.command->header->messageType);
    //   body {
    //     // The range message must be populated
    //     range {
    //       // Required bytes, the beginning of the requested range
    //       startKey: "..."
    //
    //       // Optional bool, defaults to false
    //       // True indicates that the start key should be included in the returned 
    //       // range
    //       startKeyInclusive: ...
    //
    //       // Required bytes, the end of the requested range
    //       endKey: "..."
    //
    //       // Optional bool, defaults to false
    //       // True indicates that the end key should be included in the returned 
    //       // range
    //       endKeyInclusive: ...
    //
    //       // Required int32, must be greater than 0
    //       // The maximum number of keys returned, in sorted order
    //       maxReturned: ...
    //
    //       // Optional bool, defaults to false
    //       // If true, the key range will be returned in reverse order, starting at
    //       // endKey and moving back to startKey.  For instance
    //       // if the search is startKey="j", endKey="k", maxReturned=2,
    //       // reverse=true and the keys "k0", "k1", "k2" exist
    //       // the system will return "k2" and "k1" in that order.
    //       reverse: ....
    //     }
    //   }
    // }

    TEST_ASSERT_FALSE(Operation.valueEnabled);
    TEST_ASSERT_FALSE(Operation.sendValue);
    TEST_ASSERT_NULL(Operation.entry);
    TEST_ASSERT_EQUAL_PTR(&Request, Operation.request);
    TEST_ASSERT_NULL(Operation.response);
    TEST_ASSERT_EQUAL_PTR(&Request.message.command, Request.command);
}


void test_KineticOperation_BuildGetLog_should_build_a_GetLog_request(void)
{
    LOG_LOCATION;

    KineticDeviceInfo* pInfo;

    KineticSession_IncrementSequence_Expect(&Session);

    KineticOperation_BuildGetLog(&Operation, KINETIC_DEVICE_INFO_TYPE_STATISTICS, &pInfo);

    // Get Log
    //
    // The GETLOG operation gives the client access to log information.
    // The request message must include at least one type and can have many types. The supported types are:
    //     UTILIZATIONS
    //     TEMPERATURES
    //     CAPACITIES
    //     CONFIGURATION
    //     STATISTICS
    //     MESSAGES
    //     LIMITS
    // Below we will show the message structure used to request all types in a single GETLOG request.
    // 
    // Request Message
    // ---------------
    // command {
    //   header {
    //     clusterVersion: ...
    //     identity: ...
    //     connectionID: ...
    //     sequence: ...
    //     // The messageType should be GETLOG
    //     messageType: GETLOG
    //   }
    //   body {
    //     // The body should contain a getLog message, which must have
    //     // at least one value for type. Multiple are allowed. 
    //     // Here all types are requested.
    //     getLog {
    //       type: CAPACITIES
    //       type: CONFIGURATION
    //       type: MESSAGES
    //       type: STATISTICS
    //       type: TEMPERATURES
    //       type: UTILIZATIONS
    //     }
    //   }
    // }
    // hmac: "..."
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
    TEST_ASSERT_NULL(Operation.response);
}


void test_KineticOperation_GetLogCallback_should_copy_returned_device_info_into_dynamically_allocated_info_structure(void)
{
    // KineticConnection con;
    // KineticPDU response;
    // KineticDeviceInfo* info;
    // KineticOperation op = {
    //     .connection = &con,
    //     .response = &response,
    //     .deviceInfo = &info,
    // };

    TEST_IGNORE_MESSAGE("TODO: Need to implement!")

    // KineticSerialAllocator_Create()

    // KineticStatus status = KineticOperation_GetLogCallback(&op);
}

void test_KineticOperation_BuildP2POperation_should_build_a_P2POperation_request(void)
{
    LOG_LOCATION;

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
    

    KineticSession_IncrementSequence_Expect(&Session);

    KineticOperation_BuildP2POperation(&Operation, &p2pOp);

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
    TEST_ASSERT_NULL(Operation.response);

    // This just free's the malloc'd memory and sets statuses to "invalid" (since no operation was actually performed)
    KineticOperation_P2POperationCallback(&Operation, KINETIC_STATUS_SUCCESS);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_NOT_ATTEMPTED,
        p2pOp.operations[0].resultStatus);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_NOT_ATTEMPTED,
        p2pOp.operations[1].resultStatus);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_NOT_ATTEMPTED,
        p2pOp.operations[0].chainedOperation->operations[0].resultStatus);
}
