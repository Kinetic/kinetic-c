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
#include "kinetic_client.h"

void setUp(void)
{
    SystemTestSetup(1, true);
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_KineticClient_should_process_initial_unsolicited_status_response(void)
{
    int secondsWaiting = 0, maxWaiting = 2;
    while(Fixture.session->connectionID == 0) {
        LOG0("Waiting for connection ID...");
        sleep(1);
        secondsWaiting++;
        TEST_ASSERT_TRUE_MESSAGE(secondsWaiting < maxWaiting,
            "Timed out waiting for initial unsolicited status!");
    }
    TEST_ASSERT_TRUE_MESSAGE(Fixture.session->connectionID > 0, "Invalid connection ID!");
}
