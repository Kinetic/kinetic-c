/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_session.h"
#include "kinetic.pb-c.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_client.h"
#include "mock_kinetic_pdu_unpack.h"
#include "mock_kinetic_countingsemaphore.h"
#include "mock_kinetic_resourcewaiter.h"

#include "mock_bus.h"
#include "byte_array.h"
#include <string.h>
#include <sys/time.h>

static KineticCountingSemaphore Semaphore;
static KineticSession Session;
static KineticRequest Request;
static int OperationCompleteCallbackCount;
static KineticStatus LastStatus;
static struct _KineticClient Client;
static struct bus MessageBus;

void setUp(void)
{
    memset(&Session, 0, sizeof(Session));
    Session.config = (KineticSessionConfig) {
        .host = "somehost.com",
        .port = 17,
        .clusterVersion = 6,
    };
    Client.bus = &MessageBus;
    KineticCountingSemaphore_Create_ExpectAndReturn(KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION, &Semaphore);

    KineticStatus status = KineticSession_Create(&Session, &Client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_FALSE(Session.connected);
    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);

    KineticRequest_Init(&Request, &Session);
    OperationCompleteCallbackCount = 0;
    LastStatus = KINETIC_STATUS_INVALID;
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticSession_Create_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session(void)
{
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(NULL, NULL));
}

void test_KineticSession_Create_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_client(void)
{
    KineticSession session;
    memset(&session, 0, sizeof(session));
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(&session, NULL));
}

void test_KineticSession_Create_should_allocate_and_destroy_KineticConnections(void)
{
    KineticSession session;
    memset(&session, 0, sizeof(session));

    KineticCountingSemaphore_Create_ExpectAndReturn(KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION, &Semaphore);

    KineticStatus status = KineticSession_Create(&session, &Client);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_FALSE(session.connected);
    TEST_ASSERT_EQUAL(-1, session.socket);
    TEST_ASSERT_EQUAL_INT64(0, session.sequence);
    TEST_ASSERT_EQUAL_INT64(0, session.connectionID);

    KineticCountingSemaphore_Destroy_Expect(&Semaphore);
    KineticAllocator_FreeSession_Expect(&session);

    status = KineticSession_Destroy(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticSession_Connect_should_return_KINETIC_SESSION_EMPTY_upon_NULL_session(void)
{
    KineticStatus status = KineticSession_Connect(NULL);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, status);
}

void test_KineticSession_Connect_should_report_a_failed_connection(void)
{
    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);

    KineticSocket_Connect_ExpectAndReturn("somehost.com", 17, KINETIC_SOCKET_DESCRIPTOR_INVALID);

    KineticStatus status = KineticSession_Connect(&Session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(Session.connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, Session.socket);
}

void test_KineticSession_Connect_should_report_a_failure_to_receive_register_with_client(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

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
        .connected = true,
        .socket = 24,
        .connectionID = 5,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

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
        .socket = 24,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.socket);
    Bus_RegisterSocket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expected.socket, &session, false);
    KineticSocket_Close_Expect(session.socket);

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(session.connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, session.socket);
    TEST_ASSERT_NULL(session.si);
}

void test_KineticSession_Connect_should_report_a_failure_to_receive_initialization_info_from_device(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};
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
        .connected = true,
        .socket = 24,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

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
        .connected = true,
        .socket = 24,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.socket);
    Bus_RegisterSocket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expected.socket, &session, true);
    KineticResourceWaiter_WaitTilAvailable_ExpectAndReturn(&session.connectionReady,
        KINETIC_CONNECTION_TIMEOUT_SECS, false);
    KineticSocket_Close_Expect(expected.socket);

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(session.connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, session.socket);
    TEST_ASSERT_NULL(session.si);
}

void test_KineticSession_Connect_should_connect_to_specified_host(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

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
        .connected = true,
        .socket = 24,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

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
        .connected = true,
        .socket = 24,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.socket);
    Bus_RegisterSocket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expected.socket, &session, true);
    KineticResourceWaiter_WaitTilAvailable_ExpectAndReturn(&session.connectionReady,
        KINETIC_CONNECTION_TIMEOUT_SECS, true);

    // Establish connection
    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(session.connected);
    TEST_ASSERT_EQUAL(expected.socket, session.socket);
    TEST_ASSERT_EQUAL_STRING(expected.config.host, session.config.host);
    TEST_ASSERT_EQUAL(expected.config.port, session.config.port);
    TEST_ASSERT_EQUAL_INT64(expected.config.clusterVersion, session.config.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(expected.config.identity, session.config.identity);
    TEST_ASSERT_EQUAL_ByteArray(expected.config.hmacKey, session.config.hmacKey);
}
