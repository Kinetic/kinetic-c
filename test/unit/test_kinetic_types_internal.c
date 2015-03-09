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

#include "kinetic_proto.h"
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

    TEST_ASSERT_TRUE(request.message.header.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788, request.message.header.clusterVersion);
    TEST_ASSERT_TRUE(request.message.header.has_connectionID);
    TEST_ASSERT_EQUAL_INT64(8765432, request.message.header.connectionID);
    TEST_ASSERT_TRUE(request.message.header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(KINETIC_SEQUENCE_NOT_YET_BOUND, request.message.header.sequence);
}

void test_KineticProtoStatusCode_to_KineticStatus_should_map_from_internal_to_public_type(void)
{
    // These status codes have a one-to-one mapping for clarity
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CONNECTION_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_BUSY,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SERVICE_BUSY));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_FOUND));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_VERSION_FAILURE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_MISMATCH,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_VERSION_MISMATCH));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_FAILURE,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_HMAC_FAILURE));
    // End one-to-one mappings


    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_REQUEST));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_ATTEMPTED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_HEADER_REQUIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID_REQUEST,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DATA_ERROR,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_PERM_DATA_ERROR));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INTERNAL_ERROR));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_AUTHORIZED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NOT_AUTHORIZED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_EXPIRED));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NO_SPACE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_FAILED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_LOCKED,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DEVICE_LOCKED));

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus(KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_STATUS_CODE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus(_KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_IS_INT_SIZE));
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_INVALID,
                                    KineticProtoStatusCode_to_KineticStatus((KineticProto_Command_Status_StatusCode)
                                            (KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS + 100)));
}

void test_KineticProto_Command_Synchronization_from_KineticSynchronization_should_map_from_internal_to_public_type(void)
{
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITETHROUGH,
                      KineticProto_Command_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITETHROUGH));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SYNCHRONIZATION_WRITEBACK,
                      KineticProto_Command_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_WRITEBACK));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SYNCHRONIZATION_FLUSH,
                      KineticProto_Command_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_FLUSH));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SYNCHRONIZATION_INVALID_SYNCHRONIZATION,
                      KineticProto_Command_Synchronization_from_KineticSynchronization(
                          KINETIC_SYNCHRONIZATION_INVALID));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SYNCHRONIZATION_INVALID_SYNCHRONIZATION,
                      KineticProto_Command_Synchronization_from_KineticSynchronization(
                          (KineticSynchronization)((int)KINETIC_SYNCHRONIZATION_FLUSH + 1000)));
}

void test_KineticProto_Command_Algorithm_from_KineticAlgorithm_should_map_from_public_to_internal_type(void)
{
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_SHA1,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA1));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_SHA2,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA2));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_SHA3,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_SHA3));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_CRC32,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC32));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_CRC64,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_CRC64));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Command_Algorithm_from_KineticAlgorithm(KINETIC_ALGORITHM_INVALID));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Command_Algorithm_from_KineticAlgorithm((KineticAlgorithm)1000));
    TEST_ASSERT_EQUAL(
        KINETIC_PROTO_COMMAND_ALGORITHM_INVALID_ALGORITHM,
        KineticProto_Command_Algorithm_from_KineticAlgorithm((KineticAlgorithm) - 19));
}

void test_KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type_should_convert_from_public_to_protobuf_type(void)
{
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_UTILIZATIONS,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_TEMPERATURES,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_TEMPERATURES));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_CAPACITIES,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_CAPACITIES));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG__INIT_TYPE_CONFIGURATION,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_CONFIGURATION));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_STATISTICS,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_STATISTICS));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_MESSAGES,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_MESSAGES));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_LIMITS,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(KINETIC_DEVICE_INFO_TYPE_LIMITS));

    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_INVALID_TYPE,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type((KineticLogInfo_Type)-1));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_INVALID_TYPE,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type((KineticLogInfo_Type)((int)KINETIC_DEVICE_INFO_TYPE_LIMITS + 1)));
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_GET_LOG_TYPE_INVALID_TYPE,
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type((KineticLogInfo_Type)1000));
}

void test_Copy_KineticProto_Command_Range_to_ByteBufferArray_should_copy_keys_into_byte_buffers(void)
{
    ByteBuffer buffers[5];
    ByteBufferArray array = {
        .buffers = buffers,
        .count = 5,
    };

    TEST_ASSERT_TRUE(Copy_KineticProto_Command_Range_to_ByteBufferArray(NULL, &array));
}
