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
#include <protobuf-c/protobuf-c.h>
#include "kinetic_proto.h"
#include "unity_helper.h"
#include "mock_kinetic_socket.h"
#include "kinetic_message.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include <string.h>

static KineticConnection Connection, Expected;
static const int64_t Identity = 1234;
static uint8_t Key[] = {1,2,3,4,5,6,7,8};
static const int64_t ConnectionID = 1234;
static KineticExchange Exchange;
static KineticMessage MessageOut, MessageIn;
static KineticPDU PDUOut, PDUIn;

void setUp(void)
{
    KineticConnection_Init(&Connection);
    KineticConnection_Init(&Expected);
    KineticExchange_Init(&Exchange, Identity, Key, sizeof(Key), ConnectionID, &Connection);
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

    TEST_ASSERT_FALSE(connection.Connected);
    TEST_ASSERT_TRUE(connection.Blocking);
    TEST_ASSERT_EQUAL(0, connection.Port);
    TEST_ASSERT_EQUAL(-1, connection.FileDescriptor);
    TEST_ASSERT_EQUAL_STRING("", connection.Host);
}

void test_KineticConnection_Connect_should_report_a_failed_connection(void)
{
    Connection.Connected = true;
    Connection.Blocking = true;
    Connection.Port = 1234;
    Connection.FileDescriptor = -1;
    strcpy(Connection.Host, "invalid-host.com");
    Expected = Connection;

    KineticSocket_Connect_ExpectAndReturn(Expected.Host, Expected.Port, true, -1);

    TEST_ASSERT_FALSE(KineticConnection_Connect(&Connection, "invalid-host.com", 1234, true));
    TEST_ASSERT_FALSE(Connection.Connected);
    TEST_ASSERT_EQUAL(-1, Connection.FileDescriptor);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_blocking_connection(void)
{
    KineticSocket_Connect_ExpectAndReturn("valid-host.com", 1234, true, 24);

    KineticConnection_Connect(&Connection, "valid-host.com", 1234, true);

    TEST_ASSERT_TRUE(Connection.Connected);
    TEST_ASSERT_TRUE(Connection.Blocking);
    TEST_ASSERT_EQUAL(1234, Connection.Port);
    TEST_ASSERT_EQUAL(24, Connection.FileDescriptor);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.Host);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_non_blocking_connection(void)
{
    KineticSocket_Connect_ExpectAndReturn("valid-host.com", 2345, false, 48);

    KineticConnection_Connect(&Connection, "valid-host.com", 2345, false);

    TEST_ASSERT_TRUE(Connection.Connected);
    TEST_ASSERT_FALSE(Connection.Blocking);
    TEST_ASSERT_EQUAL(2345, Connection.Port);
    TEST_ASSERT_EQUAL(48, Connection.FileDescriptor);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.Host);
}
