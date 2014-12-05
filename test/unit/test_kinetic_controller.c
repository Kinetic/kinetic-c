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
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_allocator.h"
#include <pthread.h>


// static KineticConnection Connection;
// static int64_t ConnectionID = 12345;
static KineticPDU Response;
// static KineticPDU Requests[3];
// static KineticOperation Operation;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    // KINETIC_CONNECTION_INIT(&Connection);
    // Connection.connectionID = ConnectionID;
    // KINETIC_PDU_INIT_WITH_COMMAND(&Request, &Connection);
    // KINETIC_PDU_INIT_WITH_COMMAND(&Response, &Connection);
    // KINETIC_OPERATION_INIT(&Operation, &Connection);
    // Operation.request = &Request;
}

void tearDown(void)
{
    KineticLogger_Close();
}


void test_KineticController_HandleIncomingPDU_should_process_unsolicited_response_PDUs(void)
{
    KineticConnection connection = {
        .connected = true,
        .socket = 123,
        .connectionID = 0,
    };

    KINETIC_PDU_INIT_WITH_COMMAND(&Response, &connection);
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS;
    Response.command->header->has_connectionID = true;
    Response.command->header->connectionID = 11223344;

    KineticAllocator_NewPDU_ExpectAndReturn(&connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticAllocator_FreePDU_Expect(&connection, &Response);

    KineticController_HandleIncomingPDU(&connection);

    TEST_ASSERT_EQUAL_INT64(connection.connectionID, 11223344);
}

void test_KineticController_HandleIncomingPDU_should_process_solicited_response_PDUs_without_value_payload(void)
{
    KineticConnection connection = {
        .connected = true,
        .socket = 123,
        .connectionID = 11223344,
    };

    KineticOperation op;
    KINETIC_OPERATION_INIT(&op, &connection);

    KINETIC_PDU_INIT_WITH_COMMAND(&Response, &connection);
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;

    KineticAllocator_NewPDU_ExpectAndReturn(&connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_AssociateResponseWithOperation_ExpectAndReturn(&Response, &op);
    KineticPDU_GetValueLength_ExpectAndReturn(&Response, 0);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_Complete_Expect(&op, KINETIC_STATUS_SUCCESS);

    KineticController_HandleIncomingPDU(&connection);
}

void test_KineticController_HandleIncomingPDU_should_process_solicited_response_PDUs_with_value_payload_with_no_callback_configured(void)
{
    KineticConnection connection = {
        .connected = true,
        .socket = 123,
        .connectionID = 11223344,
    };

    KineticOperation op;
    KINETIC_OPERATION_INIT(&op, &connection);
    op.callback = NULL;

    uint8_t valueData[10];
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, sizeof(valueData), 0)};
    op.entry = &entry;

    KINETIC_PDU_INIT_WITH_COMMAND(&Response, &connection);
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;

    KineticAllocator_NewPDU_ExpectAndReturn(&connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_AssociateResponseWithOperation_ExpectAndReturn(&Response, &op);
    KineticPDU_GetValueLength_ExpectAndReturn(&Response, 7);
    KineticPDU_ReceiveValue_ExpectAndReturn(connection.socket, &entry.value, 7, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_Complete_Expect(&op, KINETIC_STATUS_SUCCESS);

    KineticController_HandleIncomingPDU(&connection);
}


static int DummyOpCallbackCalls;
static KineticStatus DummyOpCallback(KineticOperation* op)
{
    assert(op);
    DummyOpCallbackCalls++;
    return KINETIC_STATUS_SUCCESS;
}

void test_KineticController_HandleIncomingPDU_should_process_solicited_response_PDUs_with_value_payload_with_callback_configured(void)
{
    KineticConnection connection = {
        .connected = true,
        .socket = 123,
        .connectionID = 11223344,
    };

    KineticOperation op;
    KINETIC_OPERATION_INIT(&op, &connection);
    op.callback = &DummyOpCallback;
    DummyOpCallbackCalls = 0;

    uint8_t valueData[10];
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, sizeof(valueData), 0)};
    op.entry = &entry;

    KINETIC_PDU_INIT_WITH_COMMAND(&Response, &connection);
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;

    KineticAllocator_NewPDU_ExpectAndReturn(&connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_AssociateResponseWithOperation_ExpectAndReturn(&Response, &op);
    KineticPDU_GetValueLength_ExpectAndReturn(&Response, 7);
    KineticPDU_ReceiveValue_ExpectAndReturn(connection.socket, &entry.value, 7, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_Complete_Expect(&op, KINETIC_STATUS_SUCCESS);

    KineticController_HandleIncomingPDU(&connection);

    TEST_ASSERT_EQUAL(1, DummyOpCallbackCalls);
}

// void test_KineticController_Init_should_create_and_kickoff_worker_threads(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_Init");
// }

// void test_KineticController_CreateOperation_should_create_a_new_operation_for_the_specified_session(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_CreateOperation");
// }

// void test_KineticController_ExecuteOperation_should_execute_the_specified_operation(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_ExecuteOperation");
// }

// void test_KineticController_Pause_should_pause_worker_threads_when_paused(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_Pause");
// }

// void test_KineticController_ReceiveThread_should_service_response_PDUs(void)
// {
//     TEST_IGNORE_MESSAGE("TODO: Add unit tests for KineticController_ReceiveThread");
// }

#if 0
void test_KineticSession_Worker_should_run_fine_while_no_data_arrives(void)
{
    LOG_LOCATION;

    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expectedConnection = {
        .connected = true,
        .socket = 24,
    };

    KineticSession expected = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {
                .data = expected.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &expectedConnection,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticConnection actualConnection = {
        .connected = false,
        .socket = -1,
    };

    Session = (KineticSession) {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = expected.config.port,
            .clusterVersion = expected.config.clusterVersion,
            .identity = expected.config.identity,
            .hmacKey = {
                .data = Session.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &actualConnection,
    };
    memcpy(Session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host,
        expected.config.port, expected.connection->socket);

    // Setup mock expectations for worker thread so it can run in IDLE mode
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();

    // Establish connection
    KineticStatus status = KineticSession_Connect(&Session);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    LOG0("Letting worker thread run for a little bit...");
    sleep(0);
    LOG0("Done allowing worker thread to execute for a bit!");
}
#endif
