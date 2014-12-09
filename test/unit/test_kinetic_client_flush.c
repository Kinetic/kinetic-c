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
#include "mock_kinetic_connection.h"
#include "mock_kinetic_controller.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}


void test_KineticClient_flush_should_get_success_if_no_writes_are_in_progress(void)
{
    KineticConnection connection;
    KineticOperation operation;
    KineticSession session = {.connection = &connection};

    KineticController_CreateOperation_ExpectAndReturn(&session, &operation);
    KineticOperation_BuildFlush_Expect(&operation);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Flush(&session, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticClient_flush_should_expose_memory_error_from_CreateOperation(void)
{
    KineticConnection connection;
    KineticSession session = {.connection = &connection};

    KineticController_CreateOperation_ExpectAndReturn(&session, NULL);
    
    KineticStatus status = KineticClient_Flush(&session, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_MEMORY_ERROR, status);
}