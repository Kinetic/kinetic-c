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

#include "unity.h"
#include "protobuf-c.h"
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "mock_kinetic_exchange.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KINETIC_OPERATION_INIT_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;

    KINETIC_OPERATION_INIT(&op, &exchange, &request, &response);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
}

void test_KineticOperation_Init_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;
    KineticExchange exchange;
    KineticPDU request, response;

    KineticOperation_Init(&op, &exchange, &request, &response);

    TEST_ASSERT_EQUAL_PTR(&exchange, op.exchange);
    TEST_ASSERT_EQUAL_PTR(&request, op.request);
    TEST_ASSERT_EQUAL_PTR(&response, op.response);
}

void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    KineticExchange exchange;
    KineticMessage requestMsg, responseMsg;
    KineticPDU request, response;
    KineticOperation op;
    KINETIC_MESSAGE_INIT(&requestMsg);
    KINETIC_MESSAGE_INIT(&responseMsg);
    KINETIC_OPERATION_INIT(&op, &exchange, &request, &response);
    request.message = &requestMsg;
    request.proto = NULL;
    response.message = NULL;
    response.proto = NULL;

    KineticExchange_ConfigureHeader_Expect(&exchange, &requestMsg.header);

    KineticOperation_BuildNoop(&op);

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
    TEST_ASSERT_TRUE(requestMsg.header.has_messagetype);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_NOOP, requestMsg.header.messagetype);
}

void test_KineticOperation_BuildPut_should_build_and_execute_a_PUT_operation(void)
{
    KineticExchange exchange;
    KineticMessage requestMsg, responseMsg;
    KineticPDU request, response;
    KineticOperation op;

    KINETIC_MESSAGE_INIT(&requestMsg);
    KINETIC_MESSAGE_INIT(&responseMsg);
    KINETIC_OPERATION_INIT(&op, &exchange, &request, &response);
    request.message = &requestMsg;
    request.proto = NULL;
    response.message = NULL;
    response.proto = NULL;
    uint8_t value[1024*1024];

    KineticExchange_ConfigureHeader_Expect(&exchange, &requestMsg.header);

    KineticOperation_BuildPut(&op, value, sizeof(value));

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
    //   }
    // }
    // hmac: "..."
    //
    TEST_ASSERT_TRUE(requestMsg.header.has_messagetype);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_PUT, requestMsg.header.messagetype);

    // Required bytes
    // The key for the value being set
    // key: "..."
    TEST_ASSERT_TRUE(requestMsg.header.has_messagetype);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_TYPE_PUT, requestMsg.header.messagetype);

    // Required bytes
    // Versions are set on objects to support optimistic locking.
    // For operations that modify data, if the dbVersion sent in the
    // request message does not match the version stored in the db, the
    // request will fail.
    // dbVersion: "..."

    // Required bytes
    // Specifies what the next version of the data will be if this
    // operation is successful.
    // newVersion: "..."

    TEST_ASSERT_EQUAL_PTR(request.value, value);
    TEST_ASSERT_EQUAL_INT64(sizeof(value), request.valueLength);
}
