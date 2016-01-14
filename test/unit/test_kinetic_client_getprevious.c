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
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
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

void test_KineticClient_GetPrevious_should_get_error_MISSING_KEY_if_called_without_key(void)
{
    KineticEntry entry;
    memset(&entry, 0, sizeof(entry));

    KineticStatus status = KineticClient_GetPrevious(&Session, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MISSING_KEY, status);
}

void test_KineticClient_GetPrevious_should_execute_GETNEXT(void)
{
    uint8_t key[] = "schlage";
    ByteBuffer KeyBuffer = ByteBuffer_Create(key, sizeof(key), sizeof(key));

    uint8_t value[1024];
    ByteBuffer ValueBuffer = ByteBuffer_Create(value, sizeof(value), 0);

    KineticEntry entry = {
        .key = KeyBuffer,
        .value = ValueBuffer,
    };
    KineticOperation operation;

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);
    KineticBuilder_BuildGetPrevious_ExpectAndReturn(&operation, &entry, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_GetPrevious(&Session, &entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_GetPrevious_should_expose_memory_errors(void)
{
    uint8_t key[] = "schlage";
    ByteBuffer KeyBuffer = ByteBuffer_Create(key, sizeof(key), sizeof(key));

    uint8_t value[1024];
    ByteBuffer ValueBuffer = ByteBuffer_Create(value, sizeof(value), 0);

    KineticEntry entry = {
        .key = KeyBuffer,
        .value = ValueBuffer,
    };

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, NULL);
    KineticStatus status = KineticClient_GetPrevious(&Session, &entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MEMORY_ERROR, status);
}
