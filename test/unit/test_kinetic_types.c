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
    KineticSessionHandle sessionHandle; (void)sessionHandle;
    KineticSession sessionConfig; (void)sessionConfig;
    KineticStatus status; (void)status;
}

void test_KineticStatus_should_have_mapped_descriptors(void)
{
    TEST_ASSERT_EQUAL(KINETIC_STATUS_COUNT, KineticStatusDescriptorCount);
}

void test_Kinetic_GetStatusDescription_should_return_appropriate_descriptions(void)
{
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_COUNT));
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE",
                             Kinetic_GetStatusDescription((KineticStatus)(int)KINETIC_STATUS_COUNT + 1000));

    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID));
    TEST_ASSERT_EQUAL_STRING("INVALID_STATUS_CODE",
                             Kinetic_GetStatusDescription((KineticStatus)(int)KINETIC_STATUS_INVALID + 1000));


    TEST_ASSERT_EQUAL_STRING("NOT_ATTEMPTED",
                             Kinetic_GetStatusDescription((KineticStatus)0));
    TEST_ASSERT_EQUAL_STRING("NOT_ATTEMPTED",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_NOT_ATTEMPTED));
    TEST_ASSERT_EQUAL_STRING("SUCCESS",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_SUCCESS));
    TEST_ASSERT_EQUAL_STRING("SESSION_EMPTY",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_SESSION_EMPTY));
    TEST_ASSERT_EQUAL_STRING("SESSION_INVALID",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_SESSION_INVALID));
    TEST_ASSERT_EQUAL_STRING("HOST_EMPTY",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_HOST_EMPTY));
    TEST_ASSERT_EQUAL_STRING("HMAC_EMPTY",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_HMAC_EMPTY));
    TEST_ASSERT_EQUAL_STRING("NO_PDUS_AVAVILABLE",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_NO_PDUS_AVAVILABLE));
    TEST_ASSERT_EQUAL_STRING("DEVICE_BUSY",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_DEVICE_BUSY));
    TEST_ASSERT_EQUAL_STRING("CONNECTION_ERROR",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_CONNECTION_ERROR));
    TEST_ASSERT_EQUAL_STRING("INVALID_REQUEST",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_INVALID_REQUEST));
    TEST_ASSERT_EQUAL_STRING("OPERATION_INVALID",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_OPERATION_INVALID));
    TEST_ASSERT_EQUAL_STRING("OPERATION_FAILED",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_OPERATION_FAILED));
    TEST_ASSERT_EQUAL_STRING("CLUSTER_MISMATCH",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_CLUSTER_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("VERSION_MISMATCH",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_VERSION_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("DATA_ERROR",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_DATA_ERROR));
    TEST_ASSERT_EQUAL_STRING("BUFFER_OVERRUN",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_BUFFER_OVERRUN));
    TEST_ASSERT_EQUAL_STRING("MEMORY_ERROR",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_MEMORY_ERROR));
    TEST_ASSERT_EQUAL_STRING("SOCKET_TIMEOUT",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_SOCKET_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("SOCKET_ERROR",
                             Kinetic_GetStatusDescription(KINETIC_STATUS_SOCKET_ERROR));
}
