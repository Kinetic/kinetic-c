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

#include <unistd.h>
#include <err.h>
#include <sys/wait.h>

#include "system_test_fixture.h"
#include "kinetic_client.h"


#define IDLE_SECONDS 10

static void child_task(void) {
    SystemTestSetup(0, false);

    sleep(IDLE_SECONDS);

    SystemTestShutDown();
    exit(0);
}

void test_idle_system_should_go_dormant(void)
{
    const int children = 16;
    for (int i = 0; i < children; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            child_task();
        } else if (pid == -1) {
            err(1, "fork");
        } else {
            /* nop */
        }
    }

    sleep(IDLE_SECONDS);

    for (int i = 0; i < children; i++) {
        int stat_loc = 0;
        wait(&stat_loc);
    }
}
