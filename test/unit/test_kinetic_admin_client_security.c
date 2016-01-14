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
#include "kinetic.pb-c.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;

extern struct ACL *ACLs;

#define TEST_DIR(F) ("test/unit/acl/" F)

void setUp(void)
{
    KineticLogger_Init("stdout", 2);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAdminClient_SetACL_should_reject_an_invalid_ACL_path(void)
{
    KineticStatus status = KineticAdminClient_SetACL(&Session, NULL);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_INVALID_REQUEST, status);
}

void test_KineticAdminClient_SetACL_should_set_an_ACL(void)
{
    KineticOperation operation;

    const char *ACL_path = TEST_DIR("ex1.json");

    KineticACL_LoadFromFile_ExpectAndReturn(ACL_path, &ACLs, ACL_OK);
    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);
    KineticBuilder_BuildSetACL_ExpectAndReturn(&operation, ACLs, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticAdminClient_SetACL(&Session, ACL_path);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
}

void test_KineticAdminClient_SetACL_should_fail_with_KINETIC_STATUS_ACL_ERROR_if_ACL_load_fails(void)
{
    const char *ACL_path = TEST_DIR("ex1.json");

    KineticACL_LoadFromFile_ExpectAndReturn(ACL_path, &ACLs, ACL_ERROR_JSON_FILE);

    KineticStatus status = KineticAdminClient_SetACL(&Session, ACL_path);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_ACL_ERROR, status);
}
