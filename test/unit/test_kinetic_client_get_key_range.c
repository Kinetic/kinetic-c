
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
#include "kinetic_client.h"
#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_operation.h"
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
static ByteArray HmacKey;
static const char* StartKeyData[KINETIC_DEFAULT_KEY_LEN];
static const char* EndKeyData[KINETIC_DEFAULT_KEY_LEN];
static ByteBuffer StartKey, EndKey;
static const int NumKeysInRange = 4;
static uint8_t KeyRangeData[NumKeysInRange][KINETIC_MAX_KEY_LEN];
static ByteBuffer Keys[NumKeysInRange];
static KineticSessionHandle DummyHandle = 1;
static KineticSessionHandle SessionHandle = KINETIC_HANDLE_INVALID;
static KineticPDU Request, Response;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false;
    Connection.connectionID = 182736; // Dummy connection ID to allow connect to complete
    HmacKey = ByteArray_CreateWithCString("some hmac key");

    // Configure start and end key buffers
    StartKey = ByteBuffer_Create(StartKeyData, sizeof(StartKeyData), sizeof(StartKeyData));
    EndKey = ByteBuffer_Create(EndKeyData, sizeof(EndKeyData), sizeof(EndKeyData));

    // Initialize buffers to hold returned keys in requested range
    for (int i = 0; i < NumKeysInRange; i++) {
        Keys[i] = ByteBuffer_Create(&KeyRangeData[i], sizeof(KeyRangeData[i]), sizeof(KeyRangeData[i]));
        char keyBuf[64];
        snprintf(keyBuf, sizeof(keyBuf), "key_range_00_%02d", i);
        ByteBuffer_AppendCString(&Keys[i], keyBuf);
    }

    KINETIC_SESSION_INIT(&Session, "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Session, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Connect(&Session, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(DummyHandle, SessionHandle);
}

void tearDown(void)
{
    KineticLogger_Close();
}

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
void test_KineticClient_GetKeyRange_should_return_a_list_of_keys_within_the_specified_range(void)
{
    LOG_LOCATION;

    ByteBuffer_AppendCString(&StartKey, "key_range_00_00");
    ByteBuffer_AppendCString(&EndKey, "key_range_00_03");

    KineticKeyRange keyRange = {
        .startKey = StartKey,
        .endKey = EndKey,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = NumKeysInRange,
        .reverse = false,
    };

    ProtobufCBinaryData protoKeysInRange[NumKeysInRange];
    for (int i = 0; i < NumKeysInRange; i++) {
        protoKeysInRange[i] = (ProtobufCBinaryData) {
            .data = Keys[i].array.data, .len = Keys[i].bytesUsed};
    }

    KineticOperation operation = {
        .connection = &Connection,
        .request = &Request,
        .response = &Response,
    };

    ByteBufferArray keyArray = {
        .buffers = &Keys[0],
        .count = NumKeysInRange
    };

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewOperation_ExpectAndReturn(&Connection, &operation);
    KineticOperation_BuildGetKeyRange_Expect(&operation, &keyRange, &keyArray);
    KineticOperation_SendRequest_ExpectAndReturn(&operation, KINETIC_STATUS_SUCCESS);
    KineticOperation_ReceiveAsync_ExpectAndReturn(&operation, KINETIC_STATUS_BUFFER_OVERRUN);

    KineticStatus status = KineticClient_GetKeyRange(DummyHandle, &keyRange, &keyArray, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);
}
