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
#include "system_test_fixture.h"
#include "kinetic_admin_client.h"

const int64_t ClusterVersion = 1981;
bool ClusterVersionSet;

void setUp(void)
{
    SystemTestSetup(1, true);
    ClusterVersionSet = false;
}

void tearDown(void)
{
    if (ClusterVersionSet) {
        KineticStatus status = KineticAdminClient_SetClusterVersion(Fixture.adminSession, 0);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    SystemTestShutDown();
}

void test_SetClusterVersion_should_succeed(void)
{
    KineticStatus status = KineticAdminClient_SetClusterVersion(Fixture.adminSession, 12);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    ClusterVersionSet = true;
}
