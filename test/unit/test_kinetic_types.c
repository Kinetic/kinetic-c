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

#include "kinetic_types.h"
#include "unity.h"
#include "unity_helper.h"

extern const int KineticStatusDescriptorCount;

void setUp(void)
{
}

void tearDown(void)
{
}

void test_kinetic_types_should_be_defined(void)
{
    ByteArray array; (void)array;
    ByteBuffer buffer; (void)buffer;
    KineticAlgorithm algorithm; (void)algorithm;
    KineticSynchronization synchronization; (void)synchronization;
    KineticSession session; (void)session;
    KineticStatus status; (void)status;
}

void test_KineticStatus_should_have_mapped_descriptors(void)
{
    TEST_ASSERT_EQUAL(KINETIC_STATUS_COUNT, KineticStatusDescriptorCount);
}

void test_Kinetic_GetStatusDescription_should_return_appropriate_descriptions(void)
{
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE", Kinetic_GetStatusDescription(KINETIC_STATUS_COUNT));
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE", Kinetic_GetStatusDescription((KineticStatus)(int)KINETIC_STATUS_COUNT + 1000));

    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE", Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID));
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE", Kinetic_GetStatusDescription((KineticStatus)(int)KINETIC_STATUS_INVALID + 1000));

    TEST_ASSERT_EQUAL_STRING("NOT_ATTEMPTED", Kinetic_GetStatusDescription((KineticStatus)0));

    TEST_ASSERT_EQUAL_STRING("NOT_ATTEMPTED", Kinetic_GetStatusDescription(KINETIC_STATUS_NOT_ATTEMPTED));
    TEST_ASSERT_EQUAL_STRING("SUCCESS", Kinetic_GetStatusDescription(KINETIC_STATUS_SUCCESS));
    TEST_ASSERT_EQUAL_STRING("SESSION_EMPTY", Kinetic_GetStatusDescription(KINETIC_STATUS_SESSION_EMPTY));
    TEST_ASSERT_EQUAL_STRING("SESSION_INVALID", Kinetic_GetStatusDescription(KINETIC_STATUS_SESSION_INVALID));
    TEST_ASSERT_EQUAL_STRING("HOST_EMPTY", Kinetic_GetStatusDescription(KINETIC_STATUS_HOST_EMPTY));
    TEST_ASSERT_EQUAL_STRING("HMAC_REQUIRED", Kinetic_GetStatusDescription(KINETIC_STATUS_HMAC_REQUIRED));
    TEST_ASSERT_EQUAL_STRING("NO_PDUS_AVAILABLE", Kinetic_GetStatusDescription(KINETIC_STATUS_NO_PDUS_AVAILABLE));
    TEST_ASSERT_EQUAL_STRING("DEVICE_BUSY", Kinetic_GetStatusDescription(KINETIC_STATUS_DEVICE_BUSY));
    TEST_ASSERT_EQUAL_STRING("CONNECTION_ERROR", Kinetic_GetStatusDescription(KINETIC_STATUS_CONNECTION_ERROR));
    TEST_ASSERT_EQUAL_STRING("INVALID_REQUEST", Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID_REQUEST));
    TEST_ASSERT_EQUAL_STRING("OPERATION_INVALID", Kinetic_GetStatusDescription(KINETIC_STATUS_OPERATION_INVALID));
    TEST_ASSERT_EQUAL_STRING("OPERATION_FAILED", Kinetic_GetStatusDescription(KINETIC_STATUS_OPERATION_FAILED));
    TEST_ASSERT_EQUAL_STRING("OPERATION_TIMEDOUT", Kinetic_GetStatusDescription(KINETIC_STATUS_OPERATION_TIMEDOUT));
    TEST_ASSERT_EQUAL_STRING("CLUSTER_MISMATCH", Kinetic_GetStatusDescription(KINETIC_STATUS_CLUSTER_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("VERSION_MISMATCH", Kinetic_GetStatusDescription(KINETIC_STATUS_VERSION_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("DATA_ERROR", Kinetic_GetStatusDescription(KINETIC_STATUS_DATA_ERROR));
    TEST_ASSERT_EQUAL_STRING("NOT_FOUND", Kinetic_GetStatusDescription(KINETIC_STATUS_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("BUFFER_OVERRUN", Kinetic_GetStatusDescription(KINETIC_STATUS_BUFFER_OVERRUN));
    TEST_ASSERT_EQUAL_STRING("MEMORY_ERROR", Kinetic_GetStatusDescription(KINETIC_STATUS_MEMORY_ERROR));
    TEST_ASSERT_EQUAL_STRING("SOCKET_TIMEOUT", Kinetic_GetStatusDescription(KINETIC_STATUS_SOCKET_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("SOCKET_ERROR", Kinetic_GetStatusDescription(KINETIC_STATUS_SOCKET_ERROR));
    TEST_ASSERT_EQUAL_STRING("MISSING_KEY", Kinetic_GetStatusDescription(KINETIC_STATUS_MISSING_KEY));
    TEST_ASSERT_EQUAL_STRING("MISSING_VALUE_BUFFER", Kinetic_GetStatusDescription(KINETIC_STATUS_MISSING_VALUE_BUFFER));
    TEST_ASSERT_EQUAL_STRING("MISSING_PIN", Kinetic_GetStatusDescription(KINETIC_STATUS_MISSING_PIN));
    TEST_ASSERT_EQUAL_STRING("SSL_REQUIRED", Kinetic_GetStatusDescription(KINETIC_STATUS_SSL_REQUIRED));
    TEST_ASSERT_EQUAL_STRING("DEVICE_LOCKED", Kinetic_GetStatusDescription(KINETIC_STATUS_DEVICE_LOCKED));
    TEST_ASSERT_EQUAL_STRING("ACL_ERROR", Kinetic_GetStatusDescription(KINETIC_STATUS_ACL_ERROR));
    TEST_ASSERT_EQUAL_STRING("NOT_AUTHORIZED", Kinetic_GetStatusDescription(KINETIC_STATUS_NOT_AUTHORIZED));
    TEST_ASSERT_EQUAL_STRING("INVALID_FILE", Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID_FILE));
    TEST_ASSERT_EQUAL_STRING("REQUEST_REJECTED", Kinetic_GetStatusDescription(KINETIC_STATUS_REQUEST_REJECTED));
    TEST_ASSERT_EQUAL_STRING("DEVICE_NAME_REQUIRED", Kinetic_GetStatusDescription(KINETIC_STATUS_DEVICE_NAME_REQUIRED));
    TEST_ASSERT_EQUAL_STRING("INVALID_LOG_TYPE", Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID_LOG_TYPE));
    TEST_ASSERT_EQUAL_STRING("SESSION_TERMINATED", Kinetic_GetStatusDescription(KINETIC_STATUS_SESSION_TERMINATED));
}
