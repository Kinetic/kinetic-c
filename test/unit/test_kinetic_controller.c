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

#include "unity_helper.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "protobuf-c.h"
#include "mock_kinetic_response.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_resourcewaiter.h"
#include <pthread.h>

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticController_needs_tests(void)
{
    TEST_IGNORE_MESSAGE("TODO: Add tests for KineticController");
}

void test_KineticController_ExecuteOperation_should_carry_out_an_operation_with_closure_specified_as_asynchronous(void)
{
    KineticSession session = {.connected = true};
    KineticRequest request;
    KineticOperation operation = {
        .session = &session,
        .request = &request,
    };
    KineticCompletionClosure closure;

    KineticSession_GetTerminationStatus_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);
    KineticOperation_SendRequest_ExpectAndReturn(&operation, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticController_ExecuteOperation(&operation, &closure);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticController_ExecuteOperation_should_carry_out_an_operation_with_no_closure_specified_as_synchronous(void)
{
    KineticSession session = {.connected = true};
    KineticRequest request;
    KineticOperation operation = {
        .session = &session,
        .request = &request,
    };
    KineticCompletionClosure closure;

    KineticSession_GetTerminationStatus_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);
    KineticOperation_SendRequest_ExpectAndReturn(&operation, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticController_ExecuteOperation(&operation, &closure);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticController_ExecuteOperation_should_report_session_terminated_if_detected(void)
{
    KineticSession session = {.connected = true};
    KineticRequest request;
    KineticOperation operation = {
        .session = &session,
        .request = &request,
    };
    KineticCompletionClosure closure;

    KineticSession_GetTerminationStatus_ExpectAndReturn(&session, KINETIC_STATUS_HMAC_FAILURE);
    KineticStatus status = KineticController_ExecuteOperation(&operation, &closure);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_TERMINATED, status);
}

void test_KineticController_ExecuteOperation_should_carry_out_an_operation_with_closure_supplied_and_report_failure_to_send(void)
{
    KineticSession session = {.connected = true};
    KineticRequest request;
    KineticOperation operation = {
        .session = &session,
        .request = &request,
    };
    KineticCompletionClosure closure;

    KineticSession_GetTerminationStatus_ExpectAndReturn(&session, KINETIC_STATUS_SUCCESS);
    KineticOperation_SendRequest_ExpectAndReturn(&operation, KINETIC_STATUS_OPERATION_INVALID);

    KineticStatus status = KineticController_ExecuteOperation(&operation, &closure);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_INVALID, status);
}
