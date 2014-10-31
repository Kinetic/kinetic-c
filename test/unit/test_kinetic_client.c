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

#include "kinetic_client.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_operation.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

static KineticSession Session;
static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;
static KineticSessionHandle DummyHandle = 1;
static KineticSessionHandle SessionHandle = KINETIC_HANDLE_INVALID;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    SessionHandle = KINETIC_HANDLE_INVALID;
}

void tearDown()
{
    KineticLogger_Close();
}

void test_KineticClient_Init_should_initialize_the_logger(void)
{
    KineticClient_Init("./some_file.log", 3);
    KineticClient_Init("stdout", 1);
    KineticClient_Init(NULL, -1);
}

static void ConnectSession(void)
{
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false; // Ensure gets set appropriately by internal connect call
    Connection.connectionID = 12374626536; // Fake connection ID to allow connection to complete for these tests
    HmacKey = ByteArray_CreateWithCString("some hmac key");
    KINETIC_SESSION_INIT(&Session, "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Session, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    // KineticConnection_ReceiveDeviceStatusMessage_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Connect(&Session, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(DummyHandle, SessionHandle);
}



void test_KineticClient_Connect_should_configure_a_session_and_connect_to_specified_host(void)
{
    ConnectSession();
}

void test_KineticClient_Connect_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session_config(void)
{
    SessionHandle = 17;

    KineticStatus status = KineticClient_Connect(NULL, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_EMPTY, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_KineticClient_Connect_should_return_KINETIC_STATUS_HOST_EMPTY_upon_NULL_host(void)
{
    ByteArray key = ByteArray_CreateWithCString("some_key");
    KineticSession config = {
        .host = "",
        .hmacKey = key,
    };
    SessionHandle = 17;

    KineticStatus status = KineticClient_Connect(&config, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HOST_EMPTY, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_KineticClient_Connect_should_return_KINETIC_STATUS_HMAC_EMPTY_upon_NULL_HMAC_key(void)
{
    ByteArray key = {.len = 4, .data = NULL};
    KineticSession config = {
        .host = "somehost.com",
        .hmacKey = key,
    };
    SessionHandle = 17;

    KineticStatus status = KineticClient_Connect(&config, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_EMPTY, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_KineticClient_Connect_should_return_false_upon_empty_HMAC_key(void)
{
    uint8_t keyData[] = {0, 1, 2, 3};
    ByteArray key = {.len = 0, .data = keyData};
    KineticSession config = {
        .host = "somehost.com",
        .hmacKey = key,
    };
    SessionHandle = 17;

    KineticStatus status = KineticClient_Connect(&config, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_EMPTY, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_KineticClient_Connect_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_handle(void)
{
    KineticSession config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    SessionHandle = 17;

    KineticStatus status = KineticClient_Connect(&config, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_EMPTY, status);
}

void test_KineticClient_Connect_should_return_KINETIC_STATUS_SESSION_INVALID_if_connection_for_handle_is_NULL(void)
{
    KineticSession config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    SessionHandle = 17;

    KineticConnection_NewConnection_ExpectAndReturn(&config, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, NULL);

    KineticStatus status = KineticClient_Connect(&config, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticClient_Connect_should_return_status_from_a_failed_connection(void)
{
    KineticSession config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    SessionHandle = 17;

    KineticConnection_NewConnection_ExpectAndReturn(&config, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_HMAC_EMPTY);
    KineticConnection_FreeConnection_Expect(&SessionHandle);

    KineticStatus status = KineticClient_Connect(&config, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_EMPTY, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}



void test_KineticClient_Disconnect_should_return_KINETIC_STATUS_CONNECTION_ERROR_upon_failure_to_get_connection_from_handle(void)
{
    SessionHandle = DummyHandle;
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, NULL);
    KineticStatus status = KineticClient_Disconnect(&SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticClient_Disconnect_should_disconnect_and_free_the_connection_associated_with_handle(void)
{
    SessionHandle = DummyHandle;
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Disconnect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    KineticConnection_FreeConnection_Expect(&SessionHandle);
    KineticStatus status = KineticClient_Disconnect(&SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}

void test_KineticClient_Disconnect_should_return_status_from_KineticConnection_upon_faileure(void)
{
    SessionHandle = DummyHandle;
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Disconnect_ExpectAndReturn(&Connection, KINETIC_STATUS_SESSION_INVALID);
    KineticConnection_FreeConnection_Expect(&SessionHandle);
    KineticStatus status = KineticClient_Disconnect(&SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_INVALID, status);
    TEST_ASSERT_EQUAL(KINETIC_HANDLE_INVALID, SessionHandle);
}
