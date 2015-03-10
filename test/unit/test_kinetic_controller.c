/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#include "unity_helper.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
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

    TEST_ASSERT_EQUAL_KineticStatus(KINTEIC_STATUS_SESSION_TERMINATED, status);
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
