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
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_hmac.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KINETIC_OPERATION_INIT_should_initialize_a_KineticOperation(void)
{
    KineticOperation op;

    KINETIC_OPERATION_INIT(op, (KineticExchange*)0x1234, (KineticMessage*)0x2345, (KineticHMAC*)0x3456);

    TEST_ASSERT_EQUAL_PTR(0x1234, op.exchange);
    TEST_ASSERT_EQUAL_PTR(0x2345, op.message);
    TEST_ASSERT_EQUAL_PTR(0x3456, op.hmac);
}

void test_KineticMessage_BuildNoop_should_build_a_NOOP_message(void)
{
    KineticMessage message;
    KineticHMAC hmac;

    KINETIC_MESSAGE_INIT(&message); // Init here with macro, since mock won't initialize the struct

    KineticMessage_Init_Expect(&message);
    KineticHMAC_Init_Expect(&hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate_Expect(&hmac, &message, key, keyLen);

    KineticMessage_BuildNoop(&message);
}
