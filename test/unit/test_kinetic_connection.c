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

#include "kinetic_connection.h"
#include "protobuf-c.h"
#include "kinetic_proto.h"
#include "unity_helper.h"
#include "mock_kinetic_socket.h"
#include "kinetic_message.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include <string.h>

static KineticConnection Connection, Expected;
static const int64_t Identity = 1234;
static const char* Key = "12345678";
static const int64_t ConnectionID = 1234;
static KineticExchange Exchange;
static KineticMessage MessageOut, MessageIn;
static KineticPDU PDUOut, PDUIn;

void setUp(void)
{
    KineticConnection_Init(&Connection);
    KineticConnection_Init(&Expected);
    KineticExchange_Init(&Exchange, Identity, Key, strlen(Key), &Connection);
    KineticMessage_Init(&MessageOut);
    KineticMessage_Init(&MessageIn);
}

void tearDown(void)
{
}

void test_KineticConnection_Init_should_create_a_default_connection_object(void)
{
    KineticConnection connection;

    KineticConnection_Init(&connection);

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_TRUE(connection.blocking);
    TEST_ASSERT_EQUAL(0, connection.port);
    TEST_ASSERT_EQUAL(-1, connection.socketDescriptor);
    TEST_ASSERT_EQUAL_STRING("", connection.host);
}

void test_KineticConnection_Connect_should_report_a_failed_connection(void)
{
    Connection.connected = true;
    Connection.blocking = true;
    Connection.port = 1234;
    Connection.socketDescriptor = -1;
    strcpy(Connection.host, "invalid-host.com");
    Expected = Connection;

    KineticSocket_Connect_ExpectAndReturn(Expected.host, Expected.port, true, -1);

    TEST_ASSERT_FALSE(KineticConnection_Connect(&Connection, "invalid-host.com", 1234, true));
    TEST_ASSERT_FALSE(Connection.connected);
    TEST_ASSERT_EQUAL(-1, Connection.socketDescriptor);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_blocking_connection(void)
{
    KineticSocket_Connect_ExpectAndReturn("valid-host.com", 1234, true, 24);

    KineticConnection_Connect(&Connection, "valid-host.com", 1234, true);

    TEST_ASSERT_TRUE(Connection.connected);
    TEST_ASSERT_TRUE(Connection.blocking);
    TEST_ASSERT_EQUAL(1234, Connection.port);
    TEST_ASSERT_EQUAL(24, Connection.socketDescriptor);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.host);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_non_blocking_connection(void)
{
    KineticSocket_Connect_ExpectAndReturn("valid-host.com", 2345, false, 48);

    KineticConnection_Connect(&Connection, "valid-host.com", 2345, false);

    TEST_ASSERT_TRUE(Connection.connected);
    TEST_ASSERT_FALSE(Connection.blocking);
    TEST_ASSERT_EQUAL(2345, Connection.port);
    TEST_ASSERT_EQUAL(48, Connection.socketDescriptor);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.host);
}
