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
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_admin_client.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_device_info.h"
#include "byte_array.h"
#include "mock_kinetic_client.h"
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_auth.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_bus.h"
#include "mock_kinetic_acl.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

KineticSession Session;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}


void test_KineticAdminClient_Init_should_delegate_to_base_client(void)
{
    KineticClient testClient;
    KineticClient* returnedClient;

    KineticClientConfig config = {
        .logLevel = 3,
    };
    KineticClient_Init_ExpectAndReturn(&config, &testClient);
    returnedClient = KineticAdminClient_Init(&config);

    TEST_ASSERT_EQUAL_PTR(&testClient, returnedClient);
}

void test_KineticAdminClient_Shutdown_should_delegate_to_base_client(void)
{
    KineticClient client;
    KineticClient_Shutdown_Expect(&client);
    KineticAdminClient_Shutdown(&client);
}

void test_KineticAdminClient_CreateSession_should_delegate_to_base_client(void)
{
    KineticClient client;
    KineticSessionConfig config = { .port = 8765 };
    KineticSession* session = &Session;

    KineticClient_CreateSession_ExpectAndReturn(&config, &client, &session, KINETIC_STATUS_CLUSTER_MISMATCH);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH,
        KineticAdminClient_CreateSession(&config, &client, &session));
}

void test_KineticAdminClient_DestroySession_should_delegate_to_base_client(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };

    KineticClient_DestroySession_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_DestroySession(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticAdminClient_GetLog_should_request_the_specified_log_data_from_the_device(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticLogInfo* info;
    KineticOperation operation;

    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildGetLog_ExpectAndReturn(&operation, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, BYTE_ARRAY_NONE, &info, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_GetLog(&session, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &info, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticAdminClient_SecureErase_should_build_and_execute_a_SECURE_ERASE_operation(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildErase_ExpectAndReturn(&operation, true, &pin, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_SecureErase(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_SecureErase_should_forward_erroneous_status_if_SSL_enabled_validation_failed(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SSL_REQUIRED);

    KineticStatus status = KineticAdminClient_SecureErase(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}




void test_KineticAdminClient_InstantErase_should_build_and_execute_an_INSTANT_SECURE_ERASE_operation(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildErase_ExpectAndReturn(&operation, false, &pin, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_InstantErase(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_InstantErase_should_forward_erroneous_status_if_SSL_enabled_validation_failed(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SSL_REQUIRED);

    KineticStatus status = KineticAdminClient_InstantErase(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}



void test_KineticAdminClient_LockDevice_should_build_and_execute_an_LOCK_with_PIN_operation(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildLockUnlock_ExpectAndReturn(&operation, true, &pin, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_LockDevice(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_LockDevice_should_forward_erroneous_status_if_SSL_enabled_validation_failed(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SSL_REQUIRED);

    KineticStatus status = KineticAdminClient_LockDevice(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}



void test_KineticAdminClient_UnlockDevice_should_build_and_execute_an_LOCK_with_PIN_operation(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildLockUnlock_ExpectAndReturn(&operation, false, &pin, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_UnlockDevice(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_UnlockDevice_should_forward_erroneous_status_if_SSL_enabled_validation_failed(void)
{
    ByteArray pin = ByteArray_CreateWithCString("abc123");
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };

    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SSL_REQUIRED);

    KineticStatus status = KineticAdminClient_UnlockDevice(&session, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}

void test_KineticAdminClient_SetClusterVersion_should_build_and_execute_operation(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;

    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildSetClusterVersion_ExpectAndReturn(&operation, 1402, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_SetClusterVersion(&session, 1402);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_UpdateFirmware_should_build_and_execute_operation(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;
    const char* path = "some/firmware/update.file";

    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildUpdateFirmware_ExpectAndReturn(&operation, path, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_UpdateFirmware(&session, path);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_UpdateFirmware_should_report_failure_if_failed_to_build(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
    };
    KineticOperation operation;
    const char* path = "some/firmware/update.file";

    KineticAllocator_NewOperation_ExpectAndReturn(&session, &operation);
    KineticBuilder_BuildUpdateFirmware_ExpectAndReturn(&operation, path, KINETIC_STATUS_INVALID_REQUEST);

    KineticStatus status = KineticAdminClient_UpdateFirmware(&session, path);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST, status);
}
