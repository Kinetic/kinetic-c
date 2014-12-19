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
#include "kinetic_admin_client.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "mock_kinetic_client.h"
#include "mock_kinetic_auth.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_operation.h"
#include "protobuf-c/protobuf-c.h"
#include <stdio.h>

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
    KineticClient_Init_Expect("console", 2);
    KineticAdminClient_Init("console", 2);
}

void test_KineticAdminClient_Shutdown_should_delegate_to_base_client(void)
{
    KineticClient_Shutdown_Expect();
    KineticAdminClient_Shutdown();
}

void test_KineticAdminClient_CreateConnection_should_delegate_to_base_client(void)
{
    KineticSession session = {.config = (KineticSessionConfig) {.port = 8765}};

    KineticClient_CreateConnection_ExpectAndReturn(&session, KINETIC_STATUS_CLUSTER_MISMATCH);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH, KineticAdminClient_CreateConnection(&session));
}

void test_KineticAdminClient_DestroyConnection_should_delegate_to_base_client(void)
{
    KineticSession session = {.config = (KineticSessionConfig) {.port = 4321}};

    KineticClient_DestroyConnection_ExpectAndReturn(&session, KINETIC_STATUS_CLUSTER_MISMATCH);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH, KineticAdminClient_DestroyConnection(&session));
}


void test_KineticAdminClient_GetLog_should_request_the_specified_log_data_from_the_device(void)
{
    KineticConnection connection;
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
        .connection = &connection
    };
    KineticDeviceInfo* info;
    KineticOperation operation;

    KineticOperation_Create_ExpectAndReturn(&session, &operation);
    KineticOperation_BuildGetLog_Expect(&operation, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &info);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_GetLog(&session, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &info, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_KineticAdminClient_InstantSecureErase_should_build_and_execute_an_INSTANT_SECURE_ERASE_operation(void)
{
    KineticConnection connection;
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
        .connection = &connection
    };
    KineticOperation operation;

    KineticAuth_EnsurePinSupplied_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticOperation_Create_ExpectAndReturn(&session, &operation);
    KineticOperation_BuildInstantSecureErase_Expect(&operation);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_InstantSecureErase(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_InstantSecureErase_should_forward_erroneous_status_if_PIN_existence_validation_failed(void)
{
    KineticConnection connection;
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
        .connection = &connection
    };

    KineticAuth_EnsurePinSupplied_ExpectAndReturn(&session.config, KINETIC_STATUS_PIN_REQUIRED);

    KineticStatus status = KineticAdminClient_InstantSecureErase(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_PIN_REQUIRED, status);
}

void test_KineticAdminClient_InstantSecureErase_should_forward_erroneous_status_if_SSL_enabled_validation_failed(void)
{
    KineticConnection connection;
    KineticSession session = {
        .config = (KineticSessionConfig) {.port = 4321},
        .connection = &connection
    };

    KineticAuth_EnsurePinSupplied_ExpectAndReturn(&session.config, KINETIC_STATUS_SUCCESS);
    KineticAuth_EnsureSslEnabled_ExpectAndReturn(&session.config, KINETIC_STATUS_SSL_REQUIRED);

    KineticStatus status = KineticAdminClient_InstantSecureErase(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}
