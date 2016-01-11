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
#include "kinetic_allocator.h"
#include "kinetic_resourcewaiter_types.h"
#include "mock_kinetic_resourcewaiter.h"
#include "mock_kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "byte_array.h"
#include "mock_protobuf-c.h"
#include "mock_kinetic_memory.h"
#include <stdlib.h>
#include <pthread.h>

KineticSessionConfig Config;
KineticSession Session;
struct bus MessageBus;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    Session = (KineticSession) {.connected = false};
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAllocator_NewSession_should_return_null_if_calloc_returns_null(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticSession), NULL);
    KineticSession * session =  KineticAllocator_NewSession(&MessageBus, &Config);
    TEST_ASSERT_NULL(session);
}

void test_KineticAllocator_NewSession_should_return_a_session_with_connected_flag_set_to_false(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticSession), &Session);
    KineticResourceWaiter_Init_Expect(&Session.connectionReady);
    KineticSession* session =  KineticAllocator_NewSession(&MessageBus, &Config);
    TEST_ASSERT_FALSE(session->connected);
}

void test_KineticAllocator_NewSession_should_return_a_session_with_a_minus1_fd(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticSession), &Session);
    KineticResourceWaiter_Init_Expect(&Session.connectionReady);
    KineticSession* session =  KineticAllocator_NewSession(&MessageBus, &Config);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQUAL(-1, session->socket);
}

void test_KineticAllocator_NewSession_should_return_a_session_pointing_to_passed_bus(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticSession), &Session);
    KineticResourceWaiter_Init_Expect(&Session.connectionReady);
    KineticSession* session = KineticAllocator_NewSession(&MessageBus, &Config);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQUAL_PTR(&MessageBus, session->messageBus);
}

void test_KineticAllocator_NewSession_should_return_a_session_with_termination_status_set_to_SUCCESS(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticSession), &Session);
    KineticResourceWaiter_Init_Expect(&Session.connectionReady);
    KineticSession* session = KineticAllocator_NewSession(&MessageBus, &Config);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, session->terminationStatus);
    TEST_ASSERT_FALSE(session->connected);
}

void test_KineticAllocator_FreeSession_should_destroy_waiter_and_free_session(void)
{
    KineticResourceWaiter_Destroy_Expect(&Session.connectionReady);
    KineticFree_Expect(&Session);
    KineticAllocator_FreeSession(&Session);
}

void test_KineticAllocator_NewKineticResponse_should_return_null_if_calloc_return_null(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticResponse) + 1234, NULL);
    KineticResponse * response = KineticAllocator_NewKineticResponse(1234);
    TEST_ASSERT_NULL(response);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_command_if_its_not_null(void)
{
    Com__Seagate__Kinetic__Proto__Command command;

    KineticResponse rsp = { .command = &command};
    protobuf_c_message_free_unpacked_Expect(&command.base, NULL);

    KineticFree_Expect(&rsp);

    KineticAllocator_FreeKineticResponse(&rsp);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_proto_if_its_not_null(void)
{
    Com__Seagate__Kinetic__Proto__Message proto;

    KineticResponse rsp = { .proto = &proto};
    protobuf_c_message_free_unpacked_Expect(&proto.base, NULL);

    KineticFree_Expect(&rsp);

    KineticAllocator_FreeKineticResponse(&rsp);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_proto_and_command_if_they_are_not_null(void)
{
    Com__Seagate__Kinetic__Proto__Message proto;
    Com__Seagate__Kinetic__Proto__Command command;

    KineticResponse rsp = {
        .proto = &proto,
        .command = &command
    };
    protobuf_c_message_free_unpacked_Expect(&command.base, NULL);
    protobuf_c_message_free_unpacked_Expect(&proto.base, NULL);

    KineticFree_Expect(&rsp);

    KineticAllocator_FreeKineticResponse(&rsp);
}

void test_KineticAllocator_NewOperation_should_return_null_if_calloc_returns_null_for_operation(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticOperation), NULL);
    KineticOperation * operation = KineticAllocator_NewOperation(&Session);
    TEST_ASSERT_NULL(operation);
}


void test_KineticAllocator_NewOperation_should_return_null_and_free_operation_if_calloc_returns_null_for_request(void)
{
    KineticOperation op;
    op.session = &Session;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticOperation), &op);
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticRequest), NULL);
    KineticFree_Expect(&op);

    KineticOperation * operation = KineticAllocator_NewOperation(&Session);

    TEST_ASSERT_NULL(operation);
}

void test_KineticAllocator_NewOperation_should_initialize_operation_and_request(void)
{
    Session.timeoutSeconds = 423;
    KineticOperation op = {.session = NULL};
    KineticRequest request;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticOperation), &op);
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticRequest), &request);

    KineticRequest_Init_Expect(&request, &Session);
    KineticOperation * operation = KineticAllocator_NewOperation(&Session);

    TEST_ASSERT_EQUAL_PTR(operation, &op);
    TEST_ASSERT_EQUAL_PTR(&Session, operation->session);
    TEST_ASSERT_EQUAL_PTR(&request, operation->request);
    TEST_ASSERT_NULL(operation->response);
    TEST_ASSERT_EQUAL(423, operation->timeoutSeconds);
}

void test_KineticAllocator_FreeOperation_should_free_request_if_it_is_not_NULL(void)
{
    KineticRequest request;
    memset(&request, 0x00, sizeof(request));

    KineticOperation op = { .request = &request };

    KineticFree_Expect(&request);
    KineticFree_Expect(&op);

    KineticAllocator_FreeOperation(&op);
}

void test_KineticAllocator_FreeOperation_should_free_response_if_its_not_null(void)
{
    KineticResponse response;
    memset(&response, 0x00, sizeof(response));

    KineticOperation op = { .response = &response };

    KineticFree_Expect(&response);
    KineticFree_Expect(&op);

    KineticAllocator_FreeOperation(&op);
}

void test_KineticAllocator_FreeP2PProtobuf_should_free_protobuf_message_P2P_operation_tree(void)
{
    TEST_IGNORE_MESSAGE("TODO: Need to test P2P protobuf free");
}
