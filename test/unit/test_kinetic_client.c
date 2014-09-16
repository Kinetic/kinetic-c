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
#include "unity.h"
#include "unity_helper.h"
#include <stdio.h>
#include "protobuf-c/protobuf-c.h"
#include "kinetic_proto.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_logger.h"
#include "mock_kinetic_operation.h"

static KineticSession Session;
static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;

void setUp(void)
{
}

void tearDown(void)
{
    if (Connection.connected)
    {
        KineticClient_Disconnect(&Session);
    }
}

static void ConnectSession(void)
{
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false; // Ensure gets set appropriately by internal connect call
    HmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("some hmac key");
    KINETIC_SESSION_INIT(&Session, "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Session, &Connection);
    Connection.session = &Session;
    KineticConnection_Connect_ExpectAndReturn(&Connection, true);

    int status = KineticClient_Connect(&Session);
    TEST_ASSERT_EQUAL(0, status);
    TEST_ASSERT_TRUE(Connection.connected);
}

void test_KineticClient_Connect_should_configure_a_session_and_connect_to_specified_host(void)
{
    ConnectSession();
}

#if 0
void test_KineticClient_Connect_should_return_false_upon_NULL_connection(void)
{
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    KineticLogger_Log_Expect("Specified KineticConnection is NULL!");
    bool success = KineticClient_Connect(NULL, "somehost.com", 321, true, 12, 34, key);

    TEST_ASSERT_FALSE(success);
}

void test_KineticClient_Connect_should_return_false_upon_NULL_host(void)
{
    KineticConnection connection;
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");

    KineticLogger_Log_Expect("Specified host is NULL!");
    bool success = KineticClient_Connect(&connection, NULL, 321, true, 12, 34, key);

    TEST_ASSERT_FALSE(success);
}

void test_KineticClient_Connect_should_return_false_upon_NULL_HMAC_key(void)
{
    KineticConnection connection;
    ByteArray key = {.len = 4, .data = NULL};

    KineticLogger_Log_Expect("Specified HMAC key is NULL!");
    bool success = KineticClient_Connect(&connection, "somehost.com", 321, true, 12, 34, key);

    TEST_ASSERT_FALSE(success);
}

void test_KineticClient_Connect_should_return_false_upon_empty_HMAC_key(void)
{
    KineticConnection connection;
    uint8_t keyData[] = {0,1,2,3};
    ByteArray key = {.len = 0, .data = keyData};

    KineticLogger_Log_Expect("Specified HMAC key is empty!");
    bool success = KineticClient_Connect(&connection, "somehost.com", 321, true, 12, 34, key);

    TEST_ASSERT_FALSE(success);
}

void test_KineticClient_Connect_should_log_a_failed_connection_and_return_false(void)
{
    KineticConnection connection;
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    KINETIC_CONNECTION_INIT(&connection);

    // Ensure appropriately updated per internal connect call result
    Connection.connected = true;
    Connection.socketDescriptor = 333;

    KineticConnection_Connect_ExpectAndReturn(&connection, "somehost.com", 123, true, 12, 34, key, false);
    KineticLogger_Log_Expect("Failed creating connection to somehost.com:123");

    TEST_ASSERT_FALSE(KineticClient_Connect(&connection, "somehost.com", 123, true, 12, 34, key));

    TEST_ASSERT_FALSE(Connection.connected);
    TEST_ASSERT_EQUAL(-1, Connection.socketDescriptor);
}


void test_KineticClient_CreateOperation_should_create_configure_and_return_a_valid_operation_instance(void)
{
    KineticConnection connection;
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("some_key");
    KINETIC_CONNECTION_INIT(&connection);
    KineticOperation op;

    KineticPDU_Init_Expect(&Request, &connection);
    KineticPDU_Init_Expect(&Response, &connection);

    op = KineticClient_CreateOperation(&connection, &Request, &Response);

    TEST_ASSERT_EQUAL_PTR(&connection, op.connection);
    TEST_ASSERT_EQUAL_PTR(&Request, op.request);
    TEST_ASSERT_EQUAL_PTR(&Request.protoData.message, op.request->proto);
    TEST_ASSERT_EQUAL_PTR(&Response, op.response);
}
#endif
