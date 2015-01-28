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

#include "kinetic_allocator.h"
#include "kinetic_resourcewaiter_types.h"
#include "mock_kinetic_resourcewaiter.h"
#include "mock_kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "byte_array.h"
#include "mock_protobuf-c.h"
#include "unity.h"
#include "unity_helper.h"
#include "mock_kinetic_memory.h"
#include <stdlib.h>
#include <pthread.h>

KineticSessionConfig Config;
KineticConnection Connection;
KineticSession Session;
struct bus MessageBus;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAllocator_NewConnection_should_return_null_if_calloc_returns_null(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticConnection), NULL);
    KineticConnection* connection =  KineticAllocator_NewConnection(&MessageBus, &Session);
    TEST_ASSERT_NULL(connection);
}

void test_KineticAllocator_NewConnection_should_return_a_connection_with_connected_flag_set_to_false(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticConnection), &Connection);
    KineticResourceWaiter_Init_Expect(&Connection.connectionReady);
    KineticConnection* connection =  KineticAllocator_NewConnection(&MessageBus, &Session);
    TEST_ASSERT_EQUAL_PTR(&Connection, connection);
    TEST_ASSERT_FALSE(connection->connected);
}

void test_KineticAllocator_NewConnection_should_return_a_connection_with_a_minus1_fd(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticConnection), &Connection);
    KineticResourceWaiter_Init_Expect(&Connection.connectionReady);
    KineticConnection* connection =  KineticAllocator_NewConnection(&MessageBus, &Session);
    TEST_ASSERT_NOT_NULL(connection);
    TEST_ASSERT_EQUAL(-1, connection->socket);
}

void test_KineticAllocator_NewConnection_should_return_a_connection_pointing_to_passed_bus(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticConnection), &Connection);
    KineticResourceWaiter_Init_Expect(&Connection.connectionReady);
    KineticConnection* connection =  KineticAllocator_NewConnection(&MessageBus, &Session);
    TEST_ASSERT_NOT_NULL(connection);
    TEST_ASSERT_EQUAL_PTR(&MessageBus, connection->messageBus);
}

void test_KineticAllocator_NewConnection_should_return_a_connection_pointing_to_passed_session(void)
{
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticConnection), &Connection);
    KineticResourceWaiter_Init_Expect(&Connection.connectionReady);
    KineticConnection* connection =  KineticAllocator_NewConnection(&MessageBus, &Session);
    TEST_ASSERT_NOT_NULL(connection);
    TEST_ASSERT_EQUAL_PTR(&Session, connection->pSession);
}

void test_KineticAllocator_FreeConnection_should_destroy_waiter_and_free_connection(void)
{
    KineticResourceWaiter_Destroy_Expect(&Connection.connectionReady);
    KineticFree_Expect(&Connection);
    KineticAllocator_FreeConnection(&Connection);
}

void test_KineticAllocator_NewKineticResponse_should_return_null_if_calloc_return_null(void)
{
    Connection.pSession = &Session;
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticResponse) + 1234, NULL);
    KineticResponse * response = KineticAllocator_NewKineticResponse(1234);
    TEST_ASSERT_NULL(response);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_command_if_its_not_null(void)
{
    KineticProto_Command command;

    KineticResponse rsp = { .command = &command};
    protobuf_c_message_free_unpacked_Expect(&command.base, NULL);

    KineticFree_Expect(&rsp);

    KineticAllocator_FreeKineticResponse(&rsp);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_proto_if_its_not_null(void)
{
    KineticProto_Message proto;

    KineticResponse rsp = { .proto = &proto};
    protobuf_c_message_free_unpacked_Expect(&proto.base, NULL);

    KineticFree_Expect(&rsp);

    KineticAllocator_FreeKineticResponse(&rsp);
}

void test_KineticAllocator_FreeKineticResponse_should_free_the_proto_and_command_if_they_are_not_null(void)
{
    KineticProto_Message proto;
    KineticProto_Command command;

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
    KineticOperation * operation = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NULL(operation);
}


void test_KineticAllocator_NewOperation_should_return_null_and_free_operation_if_calloc_returns_null_for_pdu(void)
{
    Session.connection = &Connection;
    Connection.pSession = &Session;
    KineticOperation op;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticOperation), &op);
    KineticOperation_Init_Expect(&op, &Session);
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticPDU), NULL);
    KineticFree_Expect(&op);

    KineticOperation * operation = KineticAllocator_NewOperation(&Connection);

    TEST_ASSERT_NULL(operation);
}

void test_KineticAllocator_NewOperation_should_initialize_operation_and_pdu(void)
{
    Session.connection = &Connection;
    Connection.pSession = &Session;
    KineticOperation op;
    KineticPDU pdu;

    KineticCalloc_ExpectAndReturn(1, sizeof(KineticOperation), &op);
    KineticOperation_Init_Expect(&op, &Session);
    KineticCalloc_ExpectAndReturn(1, sizeof(KineticPDU), &pdu);

    KineticPDU_InitWithCommand_Expect(&pdu, &Session);
    KineticOperation * operation = KineticAllocator_NewOperation(&Connection);

    TEST_ASSERT_NOT_NULL(operation);
}

void test_KineticAllocator_FreeOperation_should_free_request_if_its_not_null(void)
{
    KineticPDU pdu;
    memset(&pdu, 0x00, sizeof(pdu));

    KineticOperation op = { .request = &pdu };

    KineticFree_Expect(&pdu);
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
