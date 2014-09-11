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
#include "kinetic_types.h"
#include "kinetic_proto.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_logger.h"
#include "mock_kinetic_operation.h"
#include "unity.h"
#include "unity_helper.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

KineticPDU Request, Response;

void setUp(void)
{
}

void tearDown(void)
{
}

// The `DELETE` operation removes the entry for a given key. It respects the 
// same locking behavior around `dbVersion` and `force` as described in the previous sections.
//
// **Request Message**
//
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

void test_KineticClient_Delete_should_execute_DELETE_operation(void)
{
    KineticConnection connection;
    KineticProto_Command responseCommand = KINETIC_PROTO_COMMAND__INIT;
    ByteArray hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("some_hmac_key");
    KineticKeyValue metadata = {
        .key = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927"),
    };
    KINETIC_PDU_INIT(&Request, &connection);
    Response.protoData.message.proto.command = &responseCommand;
    Response.header.valueLength = 0; // Simulate no data payload in response
    Response.value.len = 1234;

    KINETIC_CONNECTION_INIT(&connection, 1234, hmacKey);
    KineticPDU_Init_Expect(&Request, &connection);
    KineticPDU_Init_Expect(&Response, &connection);
    KineticOperation operation = KineticClient_CreateOperation(&connection,
        &Request, &Response);

    KineticOperation_BuildDelete_Expect(&operation, &metadata);
    KineticPDU_Send_ExpectAndReturn(&Request, true);
    KineticPDU_Receive_ExpectAndReturn(&Response, true);
    KineticOperation_GetStatus_ExpectAndReturn(&operation, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Delete(&operation, &metadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, Response.connection);
    TEST_ASSERT_EQUAL(0, Response.header.valueLength);
    TEST_ASSERT_EQUAL(0, metadata.value.len);
    TEST_ASSERT_EQUAL(0, Response.value.len);
}
