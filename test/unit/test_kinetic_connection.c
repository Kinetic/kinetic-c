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
#include "kinetic_connection.h"
#include "protobuf-c.h"
#include "kinetic_proto.h"
#include "unity_helper.h"
#include "mock_kinetic_socket.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_logger.h"
#include "protobuf-c.h"
#include <string.h>
#include <time.h>

static KineticConnection Connection, Expected;
static const int64_t ClusterVersion = 12;
static const int64_t Identity = 1234;
static const char* Key = "12345678";
static const int64_t ConnectionID = 1234;
static KineticMessage MessageOut, MessageIn;
static KineticPDU PDUOut, PDUIn;

void setUp(void)
{
    KINETIC_CONNECTION_INIT(&Connection, Identity, Key);
    Expected = Connection;
    KINETIC_MESSAGE_INIT(&MessageOut);
    MessageIn = MessageOut;
}

void tearDown(void)
{
}

void test_KineticConnection_Init_should_create_a_default_connection_object(void)
{
    KineticConnection connection;
    time_t curTime = time(NULL);
    KINETIC_CONNECTION_INIT(&connection, Identity, Key);

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_FALSE(connection.nonBlocking);
    TEST_ASSERT_EQUAL(0, connection.port);
    TEST_ASSERT_EQUAL(-1, connection.socketDescriptor);
    TEST_ASSERT_EQUAL_STRING("", connection.host);
    // Give 1-second flexibility in the rare case that
    // we were on a second boundary
    TEST_ASSERT_INT64_WITHIN(curTime, Connection.connectionID, 1);
    TEST_ASSERT_EQUAL_INT64(0, Connection.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1234, Connection.identity);
    TEST_ASSERT_EQUAL_STRING(Key, Connection.key);
    TEST_ASSERT_EQUAL_INT64(-1, Connection.sequence);
}

void test_KineticConnection_Connect_should_report_a_failed_connection(void)
{
    Connection = (KineticConnection){
        .connected = true,
        .nonBlocking = false,
        .port = 1234,
        .socketDescriptor = -1,
        .host = "invalid-host.com",
        .key = "some_hmac_key",
        .identity = 456789,
    };
    Expected = Connection;

    KineticSocket_Connect_ExpectAndReturn(Expected.host, Expected.port, false, -1);

    bool success = KineticConnection_Connect(&Connection, "invalid-host.com", Expected.port,
        Expected.nonBlocking, Expected.clusterVersion, Expected.identity, Expected.key);

    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_FALSE(Connection.connected);
    TEST_ASSERT_EQUAL(-1, Connection.socketDescriptor);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_blocking_connection(void)
{
    Connection = (KineticConnection){
        .host = "invalid-host.com",
        .nonBlocking = true,
        .key = "invalid",
        .socketDescriptor = -1,
        .connected = false,
    };
    Expected = Connection;
    Expected.nonBlocking = false;
    Expected.port = 1234;
    Expected.clusterVersion = 17;
    Expected.identity = 12;
    Expected.socketDescriptor = 24;
    strcpy(Expected.host, "valid-host.com");
    strcpy(Expected.key, "some_hmac_key");

    KineticSocket_Connect_ExpectAndReturn(Expected.host, Expected.port, false, Expected.socketDescriptor);

    bool success = KineticConnection_Connect(&Connection, "valid-host.com", Expected.port,
        Expected.nonBlocking, Expected.clusterVersion, Expected.identity, Expected.key);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_TRUE(Connection.connected);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.host);
    TEST_ASSERT_EQUAL(Expected.port, Connection.port);
    TEST_ASSERT_FALSE(Connection.nonBlocking);
    TEST_ASSERT_EQUAL_INT64(Expected.clusterVersion, Connection.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(Expected.identity, Connection.identity);
    TEST_ASSERT_EQUAL_STRING(Expected.key, Connection.key);
    TEST_ASSERT_EQUAL(Expected.socketDescriptor, Connection.socketDescriptor);
}

void test_KineticConnection_Connect_should_connect_to_specified_host_with_a_non_blocking_connection(void)
{
    Connection = (KineticConnection){
        .connected = false,
        .nonBlocking = false,
        .port = 1234,
        .socketDescriptor = -1 ,
        .host = "invalid-host.com",
        .key = "some_hmac_key",
        .identity = 456789,
    };
    Expected = Connection;

    KineticSocket_Connect_ExpectAndReturn("valid-host.com", 2345, true, 48);

    bool success = KineticConnection_Connect(&Connection, "valid-host.com", 2345, true, ClusterVersion, Identity, Key);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_TRUE(Connection.connected);
    TEST_ASSERT_TRUE(Connection.nonBlocking);
    TEST_ASSERT_EQUAL(2345, Connection.port);
    TEST_ASSERT_EQUAL(48, Connection.socketDescriptor);
    TEST_ASSERT_EQUAL_STRING("valid-host.com", Connection.host);
}

void test_KineticConnection_IncrementSequence_should_increment_the_sequence_count(void)
{
    Connection.sequence = 57;

    KineticConnection_IncrementSequence(&Connection);

    TEST_ASSERT_EQUAL_INT64(58, Connection.sequence);

    Connection.sequence = 57;

    KineticConnection_IncrementSequence(&Connection);

    TEST_ASSERT_EQUAL_INT64(58, Connection.sequence);
}

void test_KineticConnection_ConfigureHeader_should_set_the_exchange_fields_in_the_specified_header(void)
{
    KineticProto_Header header = KINETIC_PROTO_HEADER_INIT;
    Connection.identity = 12;
    Connection.sequence = 24;
    Connection.clusterVersion = 1122334455667788;
    Connection.identity = 37;
    Connection.connectionID = 8765432;

    KineticConnection_ConfigureHeader(&Connection, &header);

    TEST_ASSERT_TRUE(header.has_clusterversion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788, header.clusterversion);
    TEST_ASSERT_TRUE(header.has_identity);
    TEST_ASSERT_EQUAL_INT64(37, header.identity);
    TEST_ASSERT_TRUE(header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(8765432, header.connectionid);
    TEST_ASSERT_TRUE(header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(24, header.sequence);
}


