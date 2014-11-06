
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
#define MAX_KEYS_RETRIEVED (4)
static uint8_t KeyRangeData[MAX_KEYS_RETRIEVED][KINETIC_MAX_KEY_LEN];
static ByteBuffer Keys[MAX_KEYS_RETRIEVED];
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
    for (int i = 0; i < MAX_KEYS_RETRIEVED; i++) {
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

// Get Log
// =======
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
void test_KineticClient_GetKeyRange_should_return_a_list_of_keys_within_the_specified_range(void)
{
    LOG_LOCATION;

    KineticOperation operation = {
        .connection = &Connection,
        .request = &Request,
        .response = &Response,
    };

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewOperation_ExpectAndReturn(&Connection, &operation);
    KineticOperation_BuildGetLog_Expect(&operation, &keyRange, &keyArray);
    KineticOperation_SendRequest_ExpectAndReturn(&operation, KINETIC_STATUS_SUCCESS);
    KineticOperation_ReceiveAsync_ExpectAndReturn(&operation, KINETIC_STATUS_BUFFER_OVERRUN);

    KineticStatus status = KineticClient_GetLog(DummyHandle, &keyRange, &keyArray, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);
}
