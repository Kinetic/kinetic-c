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

#include "kinetic_client.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_version_info.h"
#include "byte_array.h"
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_bus.h"
#include "protobuf-c/protobuf-c.h"
#include "mock_bus.h"
#include "mock_kinetic_memory.h"
#include <stdio.h>

static KineticSession Session;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;
static struct bus MessageBus;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    memset(&Session, 0, sizeof(KineticSession));
    memset(&MessageBus, 0, sizeof(MessageBus));
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_Version_should_return_pointer_to_static_version_info(void)
{
    KineticVersionInfo *info = (KineticVersionInfo *) KineticClient_Version();
    TEST_ASSERT_EQUAL_STRING(KINETIC_C_VERSION, info->version);
    TEST_ASSERT_EQUAL_STRING(KINETIC_C_PROTOCOL_VERSION, info->protocolVersion);
    TEST_ASSERT_EQUAL_STRING(KINETIC_C_REPO_HASH, info->repoCommitHash);
}

void test_KineticClient_Init_should_initialize_the_message_bus_and_return_a_new_client(void)
{
    KineticClient client;
    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 3,
    };

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), &client);
    KineticBus_Init_ExpectAndReturn(&client, &config, true);

    KineticClient * result = KineticClient_Init(&config);

    TEST_ASSERT_EQUAL(&client, result);
}

void test_KineticClient_Init_should_return_null_if_calloc_returns_null(void)
{
    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 3,
    };

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), NULL);

    KineticClient * result = KineticClient_Init(&config);

    TEST_ASSERT_NULL(result);
}

void test_KineticClient_Init_should_free_client_if_bus_init_fails(void)
{
    KineticClient client;

    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 3,
    };
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticClient), &client);
    KineticBus_Init_ExpectAndReturn(&client, &config, false);
    KineticFree_Expect(&client);

    KineticClient * result = KineticClient_Init(&config);
    TEST_ASSERT_NULL(result);
}

static void ConnectSession(void)
{
    KineticClient client;
    client.bus = &MessageBus;
    HmacKey = ByteArray_CreateWithCString("some hmac key");
    KineticSessionConfig config = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = ClusterVersion,
        .identity = Identity,
        .hmacKey = HmacKey,
    };
    Session.connected = false; // Ensure gets set appropriately by internal connect call
    Session.config = config;
    KineticSession* session;

    KineticAllocator_NewSession_ExpectAndReturn(&MessageBus, &config, &Session);
    KineticSession_Create_ExpectAndReturn(&Session, &client, KINETIC_STATUS_SUCCESS);
    KineticSession_Connect_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticClient_CreateSession_should_configure_a_session_and_connect_to_specified_host(void)
{
    ConnectSession();
}

void test_KineticClient_CreateSession_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session_config(void)
{
    KineticStatus status = KineticClient_CreateSession(NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_INVALID, status);
}

void test_KineticClient_CreateSession_should_return_KINETIC_STATUS_HOST_EMPTY_upon_NULL_host(void)
{
    KineticClient client;
    ByteArray key = ByteArray_CreateWithCString("some_key");
    KineticSessionConfig config = {
        .host = "",
        .hmacKey = key,
    };
    KineticSession* session = NULL;

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HOST_EMPTY, status);
}

void test_KineticClient_CreateSession_should_return_KINETIC_STATUS_HMAC_REQUIRED_upon_NULL_HMAC_key(void)
{
    KineticClient client;
    ByteArray key = {.len = 4, .data = NULL};
    KineticSessionConfig config = {
        .host = "somehost.com",
        .hmacKey = key,
    };
    KineticSession* session;

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticClient_CreateSession_should_return_false_upon_empty_HMAC_key(void)
{
    KineticClient client;
    uint8_t keyData[] = {0, 1, 2, 3};
    ByteArray key = {.len = 0, .data = keyData};
    KineticSessionConfig config = {
        .host = "somehost.com",
        .hmacKey = key,
    };
    KineticSession* session;

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticClient_CreateSession_should_return_MEMORY_ERROR_if_unable_to_allocate_a_session(void)
{
    KineticClient client;
    client.bus = &MessageBus;
    KineticSessionConfig config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    KineticSession* session = NULL;

    KineticAllocator_NewSession_ExpectAndReturn(&MessageBus, &config, NULL);

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MEMORY_ERROR, status);
}

void test_KineticClient_CreateSession_should_return_KINETIC_STATUS_MEMORY_ERROR_upon_NULL_connection(void)
{
    KineticClient client = {
        .bus = &MessageBus,
    };
    KineticSessionConfig config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    KineticSession* session = NULL;

    KineticAllocator_NewSession_ExpectAndReturn(&MessageBus, &config, NULL);

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MEMORY_ERROR, status);
    TEST_ASSERT_NULL(session);
}

void test_KineticClient_CreateSession_should_return_status_from_a_failed_connection(void)
{
    KineticClient client;
    client.bus = &MessageBus;
    KineticSessionConfig config = {
        .host = "somehost.com",
        .hmacKey = ByteArray_CreateWithCString("some_key"),
    };
    KineticSession* session;

    KineticAllocator_NewSession_ExpectAndReturn(&MessageBus, &config, &Session);
    KineticSession_Create_ExpectAndReturn(&Session, &client, KINETIC_STATUS_SUCCESS);
    KineticSession_Connect_ExpectAndReturn(&Session, KINETIC_STATUS_HMAC_REQUIRED);
    KineticAllocator_FreeSession_Expect(&Session);

    KineticStatus status = KineticClient_CreateSession(&config, &client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}


void test_KineticClient_DestroySession_should_disconnect_and_free_the_connection_associated_with_handle(void)
{
    KineticSession_Disconnect_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticSession_Destroy_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticStatus status = KineticClient_DestroySession(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_DestroySession_should_return_status_from_KineticSession_upon_faileure(void)
{
    KineticSession_Disconnect_ExpectAndReturn(&Session, KINETIC_STATUS_SESSION_INVALID);
    KineticSession_Destroy_ExpectAndReturn(&Session, KINETIC_STATUS_SUCCESS);
    KineticStatus status = KineticClient_DestroySession(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_INVALID, status);
}

void test_KineticClient_GetTerminationStatus_should_delegate_to_session(void)
{
    KineticSession_GetTerminationStatus_ExpectAndReturn(&Session, KINETIC_STATUS_DATA_ERROR);
    KineticStatus status = KineticClient_GetTerminationStatus(&Session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR, status);
}
