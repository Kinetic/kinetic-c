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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_pdu.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;
static KineticConnection Connection;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    Session.connection = &Connection;
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_GetNext_should_get_error_MISSING_KEY_if_called_without_key(void)
{
    KineticEntry entry;
    memset(&entry, 0, sizeof(entry));

    KineticStatus status = KineticClient_GetNext(&Session, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MISSING_KEY, status);
}

void test_KineticClient_GetNext_should_execute_GETNEXT(void)
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

    KineticController_CreateOperation_ExpectAndReturn(&Session, &operation);
    KineticOperation_BuildGetNext_Expect(&operation, &entry);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_GetNext(&Session, &entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_GetNext_should_expose_memory_errors(void)
{
    uint8_t key[] = "schlage";
    ByteBuffer KeyBuffer = ByteBuffer_Create(key, sizeof(key), sizeof(key));

    uint8_t value[1024];
    ByteBuffer ValueBuffer = ByteBuffer_Create(value, sizeof(value), 0);

    KineticEntry entry = {
        .key = KeyBuffer,
        .value = ValueBuffer,
    };

    KineticController_CreateOperation_ExpectAndReturn(&Session, NULL);
    KineticStatus status = KineticClient_GetNext(&Session, &entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MEMORY_ERROR, status);
}
