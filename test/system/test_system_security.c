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
#include "system_test_fixture.h"
#include "kinetic_admin_client.h"

#define TEST_DIR(F) ("test/unit/acl/" F)

void setUp(void)
{
    SystemTestSetup(3);
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_Secure_should_set_ACL(void)
{
    const char *ACL_path = TEST_DIR("system_test.json");
    KineticStatus res = KineticAdminClient_SetACL(Fixture.adminSession,
        ACL_path);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, res);
}
