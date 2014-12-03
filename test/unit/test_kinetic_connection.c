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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_connection.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "byte_array.h"
#include <string.h>
#include <sys/time.h>

static KineticSession Session = {
    .host = "somehost.com",
    .port = 17,
};
static KineticPDU Request, Response;
static int OperationCompleteCallbackCount;
static KineticStatus LastStatus;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    strcpy(Session.host, "somehost.com");
    Session.port = 17;
    KineticStatus status = KineticSession_Create(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NOT_NULL(Session.connection);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL_STRING(Session.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.port);
    KINETIC_PDU_INIT_WITH_COMMAND(&Request, Session.connection);
    KINETIC_PDU_INIT_WITH_COMMAND(&Response, Session.connection);
    OperationCompleteCallbackCount = 0;
    LastStatus = KINETIC_STATUS_INVALID;
}

void tearDown(void)
{
    if (Session.connection != NULL) {
        if (Session.connection->connected) {
            KineticStatus status = KineticSession_Disconnect(&Session);
            TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
            TEST_ASSERT_FALSE(Session.connection->connected);
        }
        KineticSession_Destroy(&Session);
    }
    KineticLogger_Close();
}

void test_KineticSession_Create_should_return_HANDLE_INVALID_upon_failure(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(NULL));
}

void test_KineticSession_Create_should_allocate_and_destroy_KineticConnections(void)
{
    LOG_LOCATION;
    KineticSession session;
    KineticStatus status = KineticSession_Create(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NOT_NULL(session.connection);
    TEST_ASSERT_FALSE(session.connection->connected);
    status = KineticSession_Destroy(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NULL(session.connection);
}

void test_KINETIC_CONNECTION_INIT_should_create_a_default_connection_object(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_EQUAL(0, connection.session.port);
    TEST_ASSERT_EQUAL(-1, connection.socket);
    TEST_ASSERT_EQUAL_INT64(0, connection.sequence);
    TEST_ASSERT_EQUAL_INT64(0, connection.connectionID);
    TEST_ASSERT_EQUAL_STRING("", connection.session.host);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.identity);
}

void test_KineticSession_Connect_should_return_KINETIC_SESSION_EMPTY_upon_NULL_session(void)
{
    LOG_LOCATION;

    KineticStatus status = KineticSession_Connect(NULL);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, status);
}

void test_KineticSession_Connect_should_return_KINETIC_STATUS_CONNECTION_ERROR_upon_NULL_connection(void)
{
    LOG_LOCATION;
    KineticSession session = {.connection = NULL};

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticSession_Connect_should_report_a_failed_connection(void)
{
    LOG_LOCATION;

    TEST_ASSERT_EQUAL_STRING(Session.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.port);

    KineticSocket_Connect_ExpectAndReturn("somehost.com", 17, -1);

    KineticStatus status = KineticSession_Connect(&Session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, Session.connection->socket);
}

void test_KineticSession_Connect_should_connect_to_specified_host(void)
{
    LOG_LOCATION;

    TEST_IGNORE_MESSAGE("TODO: Get me working!");

    // const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    // KineticConnection expected = (KineticConnection) {
    //     .connected = true,
    //     .socket = 24,
    //     .session = (KineticSession) {
    //         .host = "valid-host.com",
    //         .port = 1234,
    //         .clusterVersion = 17,
    //         .identity = 12,
    //         .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
    //     },
    // };
    // memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    // *Connection = (KineticConnection) {
    //     .connected = false,
    //     .socket = -1,
    //     .session = (KineticSession) {
    //         .host = "valid-host.com",
    //         .port = expected.session.port,
    //         .clusterVersion = expected.session.clusterVersion,
    //         .identity = expected.session.identity,
    //         .hmacKey = {.data = Connection->session.keyData, .len = sizeof(hmacKey)},
    //     },
    // };
    // memcpy(Connection->session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    // KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port, expected.socket);

    // // Setup mock expectations for worker thread
    // KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);

    // // Establish connection
    // KineticStatus status = KineticSession_Connect(Connection);

    // TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
    // TEST_ASSERT_TRUE(Connection->connected);
    // TEST_ASSERT_EQUAL(expected.socket, Connection->socket);

    // TEST_ASSERT_EQUAL_STRING(expected.session.host, Connection->session.host);
    // TEST_ASSERT_EQUAL(expected.session.port, Connection->session.port);
    // TEST_ASSERT_EQUAL_INT64(expected.session.clusterVersion, Connection->session.clusterVersion);
    // TEST_ASSERT_EQUAL_INT64(expected.session.identity, Connection->session.identity);
    // TEST_ASSERT_EQUAL_ByteArray(expected.session.hmacKey, Connection->session.hmacKey);
}

#if 0
void test_KineticSession_Worker_should_run_fine_while_no_data_arrives(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expected = (KineticConnection) {
        .connected = true,
        .socket = 24,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    *Connection = (KineticConnection) {
        .connected = false,
        .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = expected.session.port,
            .clusterVersion = expected.session.clusterVersion,
            .identity = expected.session.identity,
            .hmacKey = {.data = Connection->session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(Connection->session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port, expected.socket);

    // Setup mock expectations for worker thread so it can run in IDLE mode
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();

    // Establish connection
    KineticStatus status = KineticSession_Connect(Connection);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    LOG0("Letting worker thread run for a little bit...");
    sleep(0);
    LOG0("Done allowing worker thread to execute for a bit!");
}

void test_KineticSession_Worker_should_process_unsolicited_response_PDUs(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};
    const int socket = 24;
    const int connectionID = 7263514;

    KineticConnection expected = (KineticConnection) {
        .connected = true,
        .socket = socket,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    *Connection = (KineticConnection) {
        .connected = false,
        .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = expected.session.port,
            .clusterVersion = expected.session.clusterVersion,
            .identity = expected.session.identity,
            .hmacKey = {.data = Connection->session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(Connection->session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port, expected.socket);

    // Setup mock expectations for worker thread
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();

    Response.type = KINETIC_PDU_TYPE_UNSOLICITED;
    Response.command->header->connectionID = connectionID;
    Response.command->header->has_connectionID = true;
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS;
    Response.proto->has_authType = true;

    // KineticOperation_TimeoutOperations_Expect(Connection);

    // Establish connection
    KineticStatus status = KineticSession_Connect(Connection);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    // Ensure no PDUs are attempted to be read if some, but not enough, data has been received
    sleep(0);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, Connection->connectionID,
        "Connection ID should not be assigned yet!");

    // Pause worker thread to setup expectations
    KineticController_Pause(Connection, true);
    sleep(0);

    // Prepare the status PDU to be received
    KineticAllocator_NewPDU_ExpectAndReturn(Connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticAllocator_FreePDU_Expect(Connection, &Response);

    KineticOperation_TimeoutOperations_Expect(Connection);

    // Must trigger data ready last, in order for mocked simulation to work as desired
    KineticSocket_WaitUntilDataAvailable_ExpectAndReturn(socket, 100, KINETIC_WAIT_STATUS_DATA_AVAILABLE);
    KineticOperation_TimeoutOperations_Expect(Connection);

    // Make sure to return read thread to IDLE state
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();

    KineticController_Pause(Connection, false);

    // Wait for unsolicited status PDU to be received and processed...
    sleep(1);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(connectionID, Response.connection->connectionID,
        "Connection ID should have been extracted from unsolicited status PDU!");
}


struct DummyCompletionClosureData {
    KineticStatus status;
    int callbackCount;
};

void DummyCompletionCallback(KineticCompletionData* kinetic_data, void* client_data)
{
    assert(kinetic_data != NULL);
    KineticCompletionData* kdata = kinetic_data;

    assert(client_data != NULL);
    struct DummyCompletionClosureData* cdata = client_data;

    cdata->status = kdata->status;
    cdata->callbackCount++;
}

void test_KineticSession_Worker_should_process_solicited_response_PDUs(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};
    const int socket = 24;

    KineticConnection expected = (KineticConnection) {
        .connected = true,
        .socket = socket,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    *Connection = (KineticConnection) {
        .connected = false,
        .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = expected.session.port,
            .clusterVersion = expected.session.clusterVersion,
            .identity = expected.session.identity,
            .hmacKey = {.data = Connection->session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(Connection->session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port, expected.socket);


    KineticOperation op;
    KINETIC_OPERATION_INIT(&op, Connection);


    // Setup mock expectations for worker thread
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();
    Response.type = KINETIC_PDU_TYPE_RESPONSE;
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    Response.proto->has_authType = true;


    // Establish connection
    KineticStatus status = KineticSession_Connect(Connection);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    // Pause worker thread to setup expectations
    KineticController_Pause(Connection, true);
    sleep(0);

    // Prepare the status PDU to be received
    KineticAllocator_NewPDU_ExpectAndReturn(Connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_AssociateResponseWithOperation_ExpectAndReturn(&Response, &op);
    KineticPDU_GetValueLength_ExpectAndReturn(&Response, 0);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_Complete_Expect(&op, KINETIC_STATUS_SUCCESS);

    KineticOperation_TimeoutOperations_Expect(Connection);


    // Signal data has arrived so status PDU can be consumed
    KineticSocket_WaitUntilDataAvailable_ExpectAndReturn(socket, 100, KINETIC_WAIT_STATUS_DATA_AVAILABLE);
    KineticOperation_TimeoutOperations_Expect(Connection);

    // Make sure to return read thread to IDLE state
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();
    KineticController_Pause(Connection, false);

    // Wait for solicited status PDU to be received and processed...
    sleep(1);
}

void test_KineticSession_Worker_should_process_solicited_response_PDUs_with_Value_payload(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};
    const int socket = 24;

    KineticConnection expected = (KineticConnection) {
        .connected = true,
        .socket = socket,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    *Connection = (KineticConnection) {
        .connected = false,
        .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = expected.session.port,
            .clusterVersion = expected.session.clusterVersion,
            .identity = expected.session.identity,
            .hmacKey = {.data = Connection->session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(Connection->session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port, expected.socket);

    KineticOperation op;
    KINETIC_OPERATION_INIT(&op, Connection);
    uint8_t valueData[100];
    KineticEntry entry = {.value = ByteBuffer_Create(valueData, sizeof(valueData), 0)};
    op.entry = &entry;

    // Setup mock expectations for worker thread
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();
    Response.type = KINETIC_PDU_TYPE_RESPONSE;
    Response.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    Response.proto->has_authType = true;

    // Establish connection
    KineticStatus status = KineticSession_Connect(Connection);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    // Pause worker thread to setup expectations
    KineticController_Pause(Connection, true);
    sleep(0);

    // Prepare the status PDU to be received
    KineticAllocator_NewPDU_ExpectAndReturn(Connection, &Response);
    KineticPDU_ReceiveMain_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_AssociateResponseWithOperation_ExpectAndReturn(&Response, &op);
    KineticPDU_GetValueLength_ExpectAndReturn(&Response, 83);
    KineticPDU_ReceiveValue_ExpectAndReturn(socket, &entry.value, 83, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticOperation_Complete_Expect(&op, KINETIC_STATUS_SUCCESS);

    KineticOperation_TimeoutOperations_Expect(Connection);

    // Signal data has arrived so status PDU can be consumed
    KineticSocket_WaitUntilDataAvailable_ExpectAndReturn(socket, 100, KINETIC_WAIT_STATUS_DATA_AVAILABLE);
    KineticOperation_TimeoutOperations_Expect(Connection);

    // Make sure to return read thread to IDLE state
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);
    KineticOperation_TimeoutOperations_Ignore();
    KineticController_Pause(Connection, false);

    // Wait for solicited status PDU to be received and processed...
    sleep(1);
}

void test_KineticSession_IncrementSequence_should_increment_the_sequence_count(void)
{
    LOG_LOCATION;
    Connection->sequence = 57;
    KineticSession_IncrementSequence(Connection);
    TEST_ASSERT_EQUAL_INT64(58, Connection->sequence);

    Connection->sequence = 0;
    KineticSession_IncrementSequence(Connection);
    TEST_ASSERT_EQUAL_INT64(1, Connection->sequence);
}
#endif
