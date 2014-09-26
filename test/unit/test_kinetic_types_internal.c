/*
* kinetic-c-client
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

#include "kinetic_types_internal.h"
#include "unity.h"
#include "unity_helper.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_kinetic_internal_types_should_be_defined(void)
{
    KineticPDU pdu; (void)pdu;
    KineticConnection connection; (void)connection;
    KineticHMAC hmac; (void)hmac;
    KineticMessage msg; (void)msg;
    KineticPDUHeader pduHeader; (void)pduHeader;
    KineticOperation operation; (void)operation;

    TEST_ASSERT_EQUAL(-1, KINETIC_SOCKET_DESCRIPTOR_INVALID);
}

void test_KineticProto_Algorithm_from_KineticAlgorithm_should_map_from_public_to_internal_type(void)
{
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_SHA1,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA1));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_SHA2,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA2));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_SHA3,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA3));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_CRC32,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC32));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_CRC64,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC64));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_INVALID));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Algorithm_from_KineticAlgorithm((KineticAlgorithm)1000));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Algorithm_from_KineticAlgorithm((KineticAlgorithm) - 19));
}
