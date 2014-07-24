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
#include <protobuf-c/protobuf-c.h>
#include "kinetic_types.h"
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "mock_kinetic_exchange.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KINETIC_OPERATION_INIT_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;

    KINETIC_OPERATION_INIT(&op, (KineticExchange*)0x1234, (KineticMessage*)0x2345);

    TEST_ASSERT_EQUAL_PTR(0x1234, op.exchange);
    TEST_ASSERT_EQUAL_PTR(0x2345, op.message);
}

void test_KineticOperation_Init_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;

    KineticOperation_Init(&op, (KineticExchange*)0x1234, (KineticMessage*)0x2345);

    TEST_ASSERT_EQUAL_PTR(0x1234, op.exchange);
    TEST_ASSERT_EQUAL_PTR(0x2345, op.message);
}

void test_KineticOperation_BuildNoop_should_build_and_execute_a_NOOP_operation(void)
{
    KineticMessage message;
    KineticOperation op;
    KineticExchange exchange;
    KINETIC_MESSAGE_INIT(&message);
    KINETIC_OPERATION_INIT(&op, &exchange, &message);

    KineticMessage_Init_Expect(&message);
    KineticExchange_ConfigureHeader_Expect(&exchange, &message.header);

    KineticOperation_BuildNoop(&op);

    TEST_IGNORE_MESSAGE("Need to populate with NOOP fields/content!");
}
