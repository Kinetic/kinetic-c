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
#include <unistd.h>
#include <err.h>

#include "system_test_fixture.h"
#include "kinetic_client.h"

static SystemTestFixture Fixture;

#define IDLE_SECONDS 10

void setUp(void)
{
}

void tearDown(void)
{
}

static void child_task(void) {
    SystemTestSetup(&Fixture, 0);

    sleep(IDLE_SECONDS);

    SystemTestTearDown(&Fixture);
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
