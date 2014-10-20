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
#include "mock_kinetic_socket.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_allocator.h"
#include "byte_array.h"
#include <string.h>
#include <time.h>

static KineticConnection* Connection;
static KineticSessionHandle SessionHandle;
static const KineticSession SessionConfig = {
    .host = "somehost.com",
    .port = 17,
    .nonBlocking = false,
};
static KineticPDU UnsolicitedPDU;

void setUp(void)
{
    KineticLogger_Init("stdout");
    SessionHandle = KineticConnection_NewConnection(&SessionConfig);
    TEST_ASSERT_TRUE(SessionHandle > KINETIC_HANDLE_INVALID);
    Connection = KineticConnection_FromHandle(SessionHandle);
    TEST_ASSERT_NOT_NULL(Connection);
    TEST_ASSERT_FALSE(Connection->connected);
}

void tearDown(void)
{
    if (SessionHandle != KINETIC_HANDLE_INVALID) {
        if (Connection->connected) {
            KineticStatus status = KineticConnection_Disconnect(Connection);
            TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
            TEST_ASSERT_FALSE(Connection->connected);
        }
        KineticConnection_FreeConnection(&SessionHandle);
    }
}

void test_KineticConnection_NewConnection_should_return_HANDLE_INVALID_upon_failure(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, KineticConnection_NewConnection(NULL));
}

void test_KineticConnection_NewConnection_should_allocate_a_new_KineticConnection(void)
{
    LOG_LOCATION;
    KineticSessionHandle handle = KineticConnection_NewConnection(&SessionConfig);
    TEST_ASSERT_TRUE(handle > KINETIC_HANDLE_INVALID);
    KineticConnection* connection = KineticConnection_FromHandle(handle);
    TEST_ASSERT_NOT_NULL(connection);
    KineticConnection_FreeConnection(&handle);
}

void test_KineticConnection_Init_should_create_a_default_connection_object(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    time_t curTime = time(NULL);
    KINETIC_CONNECTION_INIT(&connection);

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_EQUAL(0, connection.session.port);
    TEST_ASSERT_EQUAL(-1, connection.socket);
    TEST_ASSERT_EQUAL_INT64(0, connection.sequence);
    // Give 1-second flexibility in the rare case that
    // we were on a second boundary
    TEST_ASSERT_INT64_WITHIN(curTime, connection.connectionID, 1);

    TEST_ASSERT_EQUAL_STRING("", connection.session.host);
    TEST_ASSERT_FALSE(connection.session.nonBlocking);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, connection.session.identity);

    TEST_ASSERT_FALSE(Connection->connected);
}

void test_KineticConnection_Connect_should_report_a_failed_connection(void)
{
    LOG_LOCATION;
    KineticSocket_Connect_ExpectAndReturn(SessionConfig.host,
                                          SessionConfig.port, SessionConfig.nonBlocking, -1);

    KineticStatus status = KineticConnection_Connect(Connection);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(Connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, Connection->socket);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_blocking_connection(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expected = (KineticConnection) {
        .connected = true,
         .socket = 24,
        .session = (KineticSession) {
            .host = "valid-host.com",
             .port = 1234,
              .nonBlocking = false,
               .clusterVersion = 17,
                .identity = 12,
                 .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticConnection connection = (KineticConnection) {
        .connected = false,
         .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
             .port = expected.session.port,
              .nonBlocking = false,
               .clusterVersion = expected.session.clusterVersion,
                .identity = expected.session.identity,
                 .hmacKey = {.data = connection.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(connection.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port,
                                          expected.session.nonBlocking, expected.socket);

    KineticStatus status = KineticConnection_Connect(&connection);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(connection.connected);
    TEST_ASSERT_EQUAL(expected.socket, connection.socket);

    TEST_ASSERT_EQUAL_STRING(expected.session.host, connection.session.host);
    TEST_ASSERT_EQUAL(expected.session.port, connection.session.port);
    TEST_ASSERT_EQUAL(expected.session.nonBlocking, connection.session.nonBlocking);
    TEST_ASSERT_EQUAL_INT64(expected.session.clusterVersion, connection.session.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(expected.session.identity, connection.session.identity);
    TEST_ASSERT_EQUAL_ByteArray(expected.session.hmacKey, connection.session.hmacKey);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_non_blocking_connection(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expected = (KineticConnection) {
        .connected = true,
         .socket = 24,
        .session = (KineticSession) {
            .host = "valid-host.com",
             .port = 1234,
              .nonBlocking = true,
               .clusterVersion = 17,
                .identity = 12,
                 .hmacKey = {.data = expected.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(expected.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticConnection connection = (KineticConnection) {
        .connected = false,
         .socket = -1,
        .session = (KineticSession) {
            .host = "valid-host.com",
             .port = expected.session.port,
              .nonBlocking = true,
               .clusterVersion = expected.session.clusterVersion,
                .identity = expected.session.identity,
                 .hmacKey = {.data = connection.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(connection.session.hmacKey.data, hmacKey, expected.session.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.session.host, expected.session.port,
                                          expected.session.nonBlocking, expected.socket);

    KineticStatus status = KineticConnection_Connect(&connection);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(connection.connected);
    TEST_ASSERT_EQUAL(expected.socket, connection.socket);

    TEST_ASSERT_EQUAL_STRING(expected.session.host, connection.session.host);
    TEST_ASSERT_EQUAL(expected.session.port, connection.session.port);
    TEST_ASSERT_EQUAL(expected.session.nonBlocking, connection.session.nonBlocking);
    TEST_ASSERT_EQUAL_INT64(expected.session.clusterVersion, connection.session.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(expected.session.identity, connection.session.identity);
    TEST_ASSERT_EQUAL_ByteArray(expected.session.hmacKey, connection.session.hmacKey);
}

void test_KineticConnection_ReceiveDeviceStatusMessage_should_receive_device_status_message_and_obtain_connection_ID(void)
{
    LOG_LOCATION;
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};
    KineticConnection connection = (KineticConnection) {
        .connected = false,
        .socket = 8,
        .connectionID = 0,
        .session = (KineticSession) {
            .host = "valid-host.com",
            .port = 1234,
            .nonBlocking = true,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {.data = connection.session.keyData, .len = sizeof(hmacKey)},
        },
    };
    memcpy(connection.session.hmacKey.data, hmacKey, connection.session.hmacKey.len);

    // Kinetic Protobuf:
    //   authType: UNSOLICITEDSTATUS
    //   commandBytes {
    //     header {
    //       clusterVersion: 0
    //       connectionID: 1412866010427
    //     }
    //     body {
    //     }
    //     status {
    //       code: SUCCESS
    //     }
    //   }
    KINETIC_PDU_INIT_WITH_COMMAND(&UnsolicitedPDU, &connection);
    UnsolicitedPDU.proto->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS;
    UnsolicitedPDU.proto->hmacAuth = NULL;
    UnsolicitedPDU.proto->pinAuth = NULL;
    UnsolicitedPDU.command->header->clusterVersion = 5;
    UnsolicitedPDU.command->header->connectionID = 2233445566;
    UnsolicitedPDU.command->header->has_sequence = false;
    UnsolicitedPDU.command->header->has_messageType = false;

    KineticAllocator_NewPDU_ExpectAndReturn(&connection.pdus, &connection, &UnsolicitedPDU);
    KineticPDU_Receive_ExpectAndReturn(&UnsolicitedPDU, KINETIC_STATUS_SUCCESS);
    KineticAllocator_FreePDU_Expect(&connection.pdus, &UnsolicitedPDU);

    KineticStatus status = KineticConnection_ReceiveDeviceStatusMessage(&connection);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_INT64(2233445566, connection.connectionID);
}

void test_KineticConnection_IncrementSequence_should_increment_the_sequence_count(void)
{
    LOG_LOCATION;
    Connection->sequence = 57;
    KineticConnection_IncrementSequence(Connection);
    TEST_ASSERT_EQUAL_INT64(58, Connection->sequence);

    Connection->sequence = 0;
    KineticConnection_IncrementSequence(Connection);
    TEST_ASSERT_EQUAL_INT64(1, Connection->sequence);
}
