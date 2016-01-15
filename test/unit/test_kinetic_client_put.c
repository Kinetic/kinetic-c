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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_bus.h"
#include "mock_kinetic_memory.h"
#include "mock_kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_Put_should_execute_PUT_operation(void)
{
    ByteArray value = ByteArray_CreateWithCString("Four score, and seven years ago");
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(value)};
    KineticOperation operation;
    operation.session = &Session;

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);
    KineticBuilder_BuildPut_ExpectAndReturn(&operation, &entry, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_VERSION_MISMATCH);

    KineticStatus status = KineticClient_Put(&Session, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_MISMATCH, status);
}


void test_KineticClient_Put_should_allow_NULL_pointer_to_value_data_if_length_is_zero(void)
{
    ByteArray value = {.data = NULL, .len = 0};
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(value)};
    KineticOperation operation;
    operation.session = &Session;

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);
    KineticBuilder_BuildPut_ExpectAndReturn(&operation, &entry, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_VERSION_MISMATCH);

    KineticStatus status = KineticClient_Put(&Session, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_VERSION_MISMATCH, status);
}

void test_KineticClient_Put_should_return_BUFFER_OVERRUN_if_object_value_too_long(void)
{
    ByteArray value = ByteArray_CreateWithCString("Four score, and seven years ago");
    KineticEntry entry = {.value = ByteBuffer_CreateWithArray(value)};
    KineticOperation operation;
    operation.session = &Session;

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);

    KineticBuilder_BuildPut_ExpectAndReturn(&operation, &entry, KINETIC_STATUS_BUFFER_OVERRUN);
    KineticAllocator_FreeOperation_Expect(&operation);

    KineticStatus status = KineticClient_Put(&Session, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);
}
