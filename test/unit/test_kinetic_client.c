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
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_operation.h"
#include "protobuf-c/protobuf-c.h"
#include "mock_bus.h"
#include "mock_kinetic_memory.h"
#include <stdio.h>

static KineticSession Session;
static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    memset(&Session, 0, sizeof(KineticSession));
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_Init_should_initialize_the_message_bus_and_return_a_new_client(void)
{
    KineticClient client;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), &client);
    KineticPDU_InitBus_ExpectAndReturn(0, &client, true);

    KineticClient * result = KineticClient_Init("./some_file.log", 1);
    TEST_ASSERT_EQUAL(&client, result);
}

void test_KineticClient_Init_should_return_null_if_calloc_returns_null(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), NULL);

    KineticClient * result = KineticClient_Init("./some_file.log", 3);
    TEST_ASSERT_NULL(result);
}

void test_KineticClient_Init_should_free_client_if_bus_init_fails(void)
{
    KineticClient client;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), &client);
    KineticPDU_InitBus_ExpectAndReturn(2, &client, false);
    KineticFree_Expect(&client);

    KineticClient * result = KineticClient_Init("./some_file.log", 3);
    TEST_ASSERT_NULL(result);
}

static void ConnectSession(void)
{
    KineticClient client;
    Connection.connected = false; // Ensure gets set appropriately by internal connect call
    HmacKey = ByteArray_CreateWithCString("some hmac key");
    KINETIC_SESSION_INIT(&Session, "somehost.com", ClusterVersion, Identity, HmacKey);
    Session.connection = &Connection;

    KineticSession_Create_ExpectAndReturn(&Session, &client, KINETIC_STATUS_SUCCESS);
    KineticSession_Connect_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_CreateConnection(&Session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticClient_CreateConnection_should_configure_a_session_and_connect_to_specified_host(void)
{
    ConnectSession();
}

void test_KineticClient_CreateConnection_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session_config(void)
{
    KineticStatus status = KineticClient_CreateConnection(NULL, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_EMPTY, status);
}

void test_KineticClient_CreateConnection_should_return_KINETIC_STATUS_HOST_EMPTY_upon_NULL_host(void)
{
    KineticClient client;
    ByteArray key = ByteArray_CreateWithCString("some_key");
    KineticSession session = {
        .config = {
            .host = "",
            .hmacKey = key,
        },
    };

    KineticStatus status = KineticClient_CreateConnection(&session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HOST_EMPTY, status);
}

void test_KineticClient_CreateConnection_should_return_KINETIC_STATUS_HMAC_REQUIRED_upon_NULL_HMAC_key(void)
{
    KineticClient client;
    ByteArray key = {.len = 4, .data = NULL};
    KineticSession session = {
        .config.host = "somehost.com",
        .config.hmacKey = key,
    };

    KineticStatus status = KineticClient_CreateConnection(&session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticClient_CreateConnection_should_return_false_upon_empty_HMAC_key(void)
{
    KineticClient client;
    uint8_t keyData[] = {0, 1, 2, 3};
    ByteArray key = {.len = 0, .data = keyData};
    KineticSession session = {
        .config.host = "somehost.com",
        .config.hmacKey = key,
    };

    KineticStatus status = KineticClient_CreateConnection(&session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticClient_CreateConnection_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_connection(void)
{
    KineticClient client;
    KineticSession session = {
        .config.host = "somehost.com",
        .config.hmacKey = ByteArray_CreateWithCString("some_key"),
        .connection = NULL,
    };

    KineticSession_Create_ExpectAndReturn(&session, &client, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_CreateConnection(&session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticClient_CreateConnection_should_return_status_from_a_failed_connection(void)
{
    KineticClient client;
    KineticSession session = {
        .config.host = "somehost.com",
        .config.hmacKey = ByteArray_CreateWithCString("some_key"),
        .connection = &Connection,
    };

    KineticSession_Create_ExpectAndReturn(&session, &client, KINETIC_STATUS_SUCCESS);
    KineticSession_Connect_ExpectAndReturn(&session, KINETIC_STATUS_HMAC_REQUIRED);
    KineticSession_Destroy_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_CreateConnection(&session, &client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}


void test_KineticClient_DestroyConnection_should_disconnect_and_free_the_connection_associated_with_handle(void)
{
    Session.connection = &Connection;
    KineticSession_Disconnect_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticSession_Destroy_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticStatus status = KineticClient_DestroyConnection(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_DestroyConnection_should_return_status_from_KineticSession_upon_faileure(void)
{
    Session.connection = &Connection;
    KineticSession_Disconnect_ExpectAndReturn(&Session, KINETIC_STATUS_SESSION_INVALID);
    KineticSession_Destroy_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticStatus status = KineticClient_DestroyConnection(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_INVALID, status);
}
