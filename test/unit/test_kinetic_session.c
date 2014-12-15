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
#include "kinetic_session.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "byte_array.h"
#include <string.h>
#include <sys/time.h>

static KineticConnection Connection;
static KineticSession Session;
static KineticPDU Request, Response;
static int OperationCompleteCallbackCount;
static KineticStatus LastStatus;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    Session.config = (KineticSessionConfig) {
        .host = "somehost.com",
        .port = 17,
    };
    KINETIC_CONNECTION_INIT(&Connection);
    KineticAllocator_NewConnection_ExpectAndReturn(&Connection);
    KineticStatus status = KineticSession_Create(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NOT_NULL(Session.connection);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);
    KineticPDU_InitWithCommand(&Request, Session.connection);
    KineticPDU_InitWithCommand(&Response, Session.connection);
    OperationCompleteCallbackCount = 0;
    LastStatus = KINETIC_STATUS_INVALID;
}

void tearDown(void)
{ LOG_LOCATION;
    KineticLogger_Close();
}

void test_KineticSession_Create_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(NULL));
}

void test_KineticSession_Create_should_allocate_and_destroy_KineticConnections(void)
{
    LOG_LOCATION;
    KineticSession session;
    KineticConnection connection;
    KineticAllocator_NewConnection_ExpectAndReturn(&connection);
    KineticStatus status = KineticSession_Create(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, session.connection);
    TEST_ASSERT_FALSE(session.connection->connected);

    KineticAllocator_FreeConnection_Expect(&connection);
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
    TEST_ASSERT_EQUAL(0, connection.session.config.port);
    TEST_ASSERT_EQUAL(-1, connection.socket);
    TEST_ASSERT_EQUAL_INT64(0, connection.sequence);
    TEST_ASSERT_EQUAL_INT64(0, connection.connectionID);
    TEST_ASSERT_EQUAL_STRING("", connection.session.config.host);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.config.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.config.identity);
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

    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);

    KineticSocket_Connect_ExpectAndReturn("somehost.com", 17, -1);
    KineticStatus status = KineticSession_Connect(&Session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, Session.connection->socket);
}

void test_KineticSession_Connect_should_connect_to_specified_host(void)
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
        .socket = expectedConnection.socket,
        .connectionID = 5,
    };

    KineticSession session = {
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
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.connection->socket);
    KineticController_Init_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);

    // Setup mock expectations for worker thread
    KineticSocket_WaitUntilDataAvailable_IgnoreAndReturn(KINETIC_WAIT_STATUS_TIMED_OUT);

    // Establish connection
    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(session.connection->connected);
    TEST_ASSERT_EQUAL(expected.connection->socket, actualConnection.socket);
    TEST_ASSERT_EQUAL_STRING(expected.config.host, session.config.host);
    TEST_ASSERT_EQUAL(expected.config.port, session.config.port);
    TEST_ASSERT_EQUAL_INT64(expected.config.clusterVersion, session.config.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(expected.config.identity, session.config.identity);
    TEST_ASSERT_EQUAL_ByteArray(expected.config.hmacKey, session.config.hmacKey);
}


void test_KineticSession_IncrementSequence_should_increment_the_sequence_count(void)
{
    LOG_LOCATION;

    KineticConnection connection;
    Session = (KineticSession) {.connection = &connection};

    Session.connection->sequence = 57;
    KineticSession_IncrementSequence(&Session);
    TEST_ASSERT_EQUAL_INT64(58, Session.connection->sequence);

    Session.connection->sequence = 0;
    KineticSession_IncrementSequence(&Session);
    TEST_ASSERT_EQUAL_INT64(1, Session.connection->sequence);
}
