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
#include "kinetic_admin_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic_device_info.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_bus.h"
#include "mock_kinetic_memory.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_resourcewaiter.h"
#include "mock_kinetic_auth.h"
#include "mock_kinetic_acl.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;
static KineticConnection Connection;

extern struct ACL *ACLs;

#define TEST_DIR(F) ("test/unit/acl/" F)

void setUp(void)
{
    KineticLogger_Init("stdout", 2);
    Session.connection = &Connection;
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAdminClient_SetACL_should_reject_an_invalid_ACL_path(void)
{
    Session.connection = &Connection;

    KineticStatus status = KineticAdminClient_SetACL(&Session, NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);
}

void test_KineticAdminClient_SetACL_should_set_an_ACL(void)
{
    Session.connection = &Connection;
    KineticOperation operation;

    const char *ACL_path = TEST_DIR("ex1.json");

    KineticACL_LoadFromFile_ExpectAndReturn(ACL_path, &ACLs, ACL_OK);
    KineticAllocator_NewOperation_ExpectAndReturn(&Connection, &operation);
    KineticBuilder_BuildSetACL_ExpectAndReturn(&operation, ACLs, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_SetACL(&Session, ACL_path);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_SetACL_should_fail_with_KINETIC_STATUS_ACL_ERROR_if_ACL_load_fails(void)
{
    Session.connection = &Connection;

    const char *ACL_path = TEST_DIR("ex1.json");

    KineticACL_LoadFromFile_ExpectAndReturn(ACL_path, &ACLs, ACL_ERROR_JSON_FILE);

    KineticStatus status = KineticAdminClient_SetACL(&Session, ACL_path);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_ACL_ERROR, status);
}
