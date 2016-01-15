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
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "kinetic_device_info.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_response.h"
#include "mock_kinetic_countingsemaphore.h"
#include "mock_kinetic_request.h"

static KineticSession Session;
static KineticRequest Request;
static KineticOperation Operation;

extern uint8_t * msg;
extern size_t msgSize;

void setUp(void)
{
    KineticLogger_Init("stdout", 1);
    Session = (KineticSession) {
        .config = (KineticSessionConfig) {
            .host = "anyhost",
            .port = KINETIC_PORT
        },
        .connectionID = 12345,
    };
    KineticRequest_Init(&Request, &Session);
    Operation = (KineticOperation) {
        .session = &Session,
        .request = &Request,
    };
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticOperation_SendRequest_should_error_out_on_lock_failure(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, false);
    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}


void test_KineticOperation_SendRequest_should_return_MEMORY_ERROR_on_command_pack_failure(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, true);
    KineticSession *session = Operation.session;
    KineticSession_GetNextSequenceCount_ExpectAndReturn(session, 12345);

    KineticRequest_PackCommand_ExpectAndReturn(Operation.request, KINETIC_REQUEST_PACK_FAILURE);
    KineticRequest_UnlockSend_ExpectAndReturn(Operation.session, true);

    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_MEMORY_ERROR, status);
}

void test_KineticOperation_SendRequest_should_return_error_status_on_authentication_failure(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, true);
    KineticSession *session = Operation.session;
    KineticSession_GetNextSequenceCount_ExpectAndReturn(session, 12345);

    KineticRequest_PackCommand_ExpectAndReturn(Operation.request, 100);
    KineticRequest_PopulateAuthentication_ExpectAndReturn(&session->config,
        Operation.request, NULL, KINETIC_STATUS_HMAC_REQUIRED);
    KineticRequest_UnlockSend_ExpectAndReturn(Operation.session, true);

    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticOperation_SendRequest_should_return_error_status_on_PackMessage_failure(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, true);
    KineticSession *session = Operation.session;
    KineticSession_GetNextSequenceCount_ExpectAndReturn(session, 12345);

    KineticRequest_PackCommand_ExpectAndReturn(Operation.request, 100);
    KineticRequest_PopulateAuthentication_ExpectAndReturn(&session->config,
        Operation.request, NULL, KINETIC_STATUS_SUCCESS);

    KineticRequest_PackMessage_ExpectAndReturn(&Operation, &msg, &msgSize,
        KINETIC_STATUS_MEMORY_ERROR);
    KineticRequest_UnlockSend_ExpectAndReturn(Operation.session, true);

    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_MEMORY_ERROR, status);
}

void test_KineticOperation_SendRequest_should_return_REQUEST_REJECTED_if_SendRequest_fails(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, true);
    KineticSession *session = Operation.session;
    KineticSession_GetNextSequenceCount_ExpectAndReturn(session, 12345);

    KineticRequest_PackCommand_ExpectAndReturn(Operation.request, 100);
    KineticRequest_PopulateAuthentication_ExpectAndReturn(&session->config,
        Operation.request, NULL, KINETIC_STATUS_SUCCESS);

    KineticRequest_PackMessage_ExpectAndReturn(&Operation, &msg, &msgSize, KINETIC_STATUS_SUCCESS);

    KineticCountingSemaphore_Take_Expect(Operation.session->outstandingOperations);

    KineticRequest_SendRequest_ExpectAndReturn(&Operation, msg, msgSize, false);
    KineticCountingSemaphore_Give_Expect(Operation.session->outstandingOperations);
    KineticRequest_UnlockSend_ExpectAndReturn(Operation.session, true);

    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_REQUEST_REJECTED, status);
}

void test_KineticOperation_SendRequest_should_acquire_and_increment_sequence_count_and_send_PDU_to_bus(void)
{
    KineticRequest_LockSend_ExpectAndReturn(Operation.session, true);
    KineticSession *session = Operation.session;
    KineticSession_GetNextSequenceCount_ExpectAndReturn(session, 12345);

    KineticRequest_PackCommand_ExpectAndReturn(Operation.request, 100);
    KineticRequest_PopulateAuthentication_ExpectAndReturn(&session->config,
        Operation.request, NULL, KINETIC_STATUS_SUCCESS);

    KineticRequest_PackMessage_ExpectAndReturn(&Operation, &msg, &msgSize, KINETIC_STATUS_SUCCESS);

    KineticCountingSemaphore_Take_Expect(Operation.session->outstandingOperations);

    KineticRequest_SendRequest_ExpectAndReturn(&Operation, msg, msgSize, true);
    KineticRequest_UnlockSend_ExpectAndReturn(Operation.session, true);

    KineticStatus status = KineticOperation_SendRequest(&Operation);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
}


