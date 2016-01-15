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

#include "kinetic.pb-c.h"
#include "kinetic_types.h"
#include "kinetic_logger.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KINETIC_SOCKET_DESCRIPTOR_INVALID_is_negative_1(void)
{
    TEST_ASSERT_EQUAL(-1, KINETIC_SOCKET_DESCRIPTOR_INVALID);
}

void test_KineticPDUHeader_should_have_correct_byte_packed_size(void)
{
    TEST_ASSERT_EQUAL(1 + 4 + 4, PDU_HEADER_LEN);
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN, sizeof(KineticPDUHeader));
}

void test_KineteicPDU_PDU_PROTO_MAX_LEN_should_be_1MB(void)
{
    TEST_ASSERT_EQUAL(1024 * 1024, PDU_PROTO_MAX_LEN);
}

void test_KineteicPDU_KINETIC_OBJ_SIZE_should_be_1MB(void)
{
    TEST_ASSERT_EQUAL(1024 * 1024, KINETIC_OBJ_SIZE);
}

void test_KineticPDU_KINETIC_OBJ_SIZE_should_be_the_sum_of_header_protobuf_and_value_max_lengths(void)
{
    TEST_ASSERT_EQUAL(PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + KINETIC_OBJ_SIZE, PDU_MAX_LEN);
}

void test_KineticRequest_Init_should_initialize_Request(void)
{
    KineticRequest request;
    KineticSession session;

    KineticRequest_Init(&request, &session);
}

void test_KineticRequest_Init_should_set_the_exchange_fields_in_the_embedded_protobuf_header(void)
{
    KineticRequest request;
    KineticSession session;

    session = (KineticSession) {
        .config = (KineticSessionConfig) {
            .clusterVersion = 1122334455667788,
            .identity = 37,
        },
        .sequence = 24,
        .connectionID = 8765432,
    };
    KineticRequest_Init(&request, &session);

    TEST_ASSERT_TRUE(request.message.header.has_clusterversion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788, request.message.header.clusterversion);
    TEST_ASSERT_TRUE(request.message.header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(8765432, request.message.header.connectionid);
    TEST_ASSERT_TRUE(request.message.header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(KINETIC_SEQUENCE_NOT_YET_BOUND, request.message.header.sequence);
}

void test_KineticProtoStatusCode_to_KineticStatus_should_map_from_internal_to_public_type(void)
{
    // These status codes have a one-to-one mapping for clarity
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__SUCCESS));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__REMOTE_CONNECTION_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_BUSY,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__SERVICE_BUSY));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_FOUND));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__VERSION_FAILURE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_MISMATCH,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__VERSION_MISMATCH));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_FAILURE,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__HMAC_FAILURE));
    // End one-to-one mappings


    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INVALID_REQUEST));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_ATTEMPTED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__HEADER_REQUIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SUCH_HMAC_ALGORITHM));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__PERM_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__PERM_DATA_ERROR));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INTERNAL_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_AUTHORIZED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NOT_AUTHORIZED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__EXPIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NO_SPACE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NESTED_OPERATION_ERRORS));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_LOCKED,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__DEVICE_LOCKED));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus(COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INVALID_STATUS_CODE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus((Com__Seagate__Kinetic__Proto__Command__Status__StatusCode)
                                            (COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__NESTED_OPERATION_ERRORS + 100)));
}

void test_Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization_should_map_from_internal_to_public_type(void)
{
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITETHROUGH,
                      Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITETHROUGH));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__WRITEBACK,
                      Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITEBACK));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__FLUSH,
                      Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_FLUSH));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__INVALID_SYNCHRONIZATION,
                      Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_INVALID));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SYNCHRONIZATION__INVALID_SYNCHRONIZATION,
                      Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                          (KineticSynchronization)((int)KINETIC_SYNCHRONIZATION_FLUSH + 1000)));
}

void test_Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm_should_map_from_public_to_internal_type(void)
{
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA1,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA1));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA2,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA2));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__SHA3,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA3));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC32,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC32));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__CRC64,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC64));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__INVALID_ALGORITHM,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_INVALID));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__INVALID_ALGORITHM,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm((KineticAlgorithm)1000));
    TEST_ASSERT_EQUAL(
        COM__SEAGATE__KINETIC__PROTO__COMMAND__ALGORITHM__INVALID_ALGORITHM,
        Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm((KineticAlgorithm) - 19));
}

void test_KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type_should_convert_from_public_to_protobuf_type(void)
{
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__UTILIZATIONS,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__TEMPERATURES,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_TEMPERATURES));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__CAPACITIES,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_CAPACITIES));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__CONFIGURATION,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_CONFIGURATION));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__STATISTICS,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_STATISTICS));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__MESSAGES,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_MESSAGES));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__LIMITS,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type(KINETIC_DEVICE_INFO_TYPE_LIMITS));

    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__INVALID_TYPE,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type((KineticLogInfo_Type)-1));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__INVALID_TYPE,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type((KineticLogInfo_Type)((int)KINETIC_DEVICE_INFO_TYPE_LIMITS + 1)));
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__TYPE__INVALID_TYPE,
        KineticLogInfo_Type_to_Com__Seagate__Kinetic__Proto__Command__GetLog__Type((KineticLogInfo_Type)1000));
}

void test_Copy_Com__Seagate__Kinetic__Proto__Command__Range_to_ByteBufferArray_should_copy_keys_into_byte_buffers(void)
{
    ByteBuffer buffers[5];
    ByteBufferArray array = {
        .buffers = buffers,
        .count = 5,
    };

    TEST_ASSERT_TRUE(Copy_Com__Seagate__Kinetic__Proto__Command__Range_to_ByteBufferArray(NULL, &array));
}
