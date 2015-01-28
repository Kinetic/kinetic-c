
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_admin_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_device_info.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_auth.h"
#include "mock_kinetic_pdu.h"
#include "mock_kinetic_memory.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_resourcewaiter.h"

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

void test_KineticClient_GetLog_should_request_the_specified_log_data_from_the_device(void)
{
    LOG_LOCATION;

    KineticDeviceInfo* info;
    KineticOperation operation;

    KineticAllocator_NewOperation_ExpectAndReturn(&Connection, &operation);
    KineticOperation_BuildGetLog_Expect(&operation, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &info);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_GetLog(&Session, KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS, &info, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
