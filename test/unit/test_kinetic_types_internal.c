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

#include "kinetic_proto.h"
#include "kinetic_types.h"
#include "kinetic_logger.h"
#include "protobuf-c/protobuf-c.h"
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

void test_KineticProtoStatusCode_to_KineticStatus_should__map_from_internal_to_public_type(void)
{
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_BUSY,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_REQUEST));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_HEADER_REQUIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_HMAC_FAILURE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NOT_FOUND));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_FAILURE,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_FAILURE,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_FAILURE));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_INTERNAL_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NOT_AUTHORIZED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_EXPIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NO_SPACE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus(_KINETIC_PROTO_STATUS_STATUS_CODE_IS_INT_SIZE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus((KineticProto_Status_StatusCode)
                                            (KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS + 100)));
}

void test_KineticProto_Synchronization_from_KineticSynchronization_should_map_from_internal_to_public_type(void)
{
    TEST_ASSERT_EQUAL(KINETIC_PROTO_SYNCHRONIZATION_WRITETHROUGH,
                      KineticProto_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITETHROUGH));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_SYNCHRONIZATION_WRITEBACK,
                      KineticProto_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITEBACK));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_SYNCHRONIZATION_FLUSH,
                      KineticProto_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_FLUSH));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_SYNCHRONIZATION_INVALID_SYNCHRONIZATION,
                      KineticProto_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_INVALID));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_SYNCHRONIZATION_INVALID_SYNCHRONIZATION,
                      KineticProto_Synchronization_from_KineticSynchronization(
                          (KineticSynchronization)((int)KINETIC_SYNCHRONIZATION_FLUSH + 1000)));
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
