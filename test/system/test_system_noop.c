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
#include "kinetic_client.h"

static SystemTestFixture Fixture;

void setUp(void)
{
    SystemTestSetup(&Fixture, 2);
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}


void test_NoOp_should_succeed(void)
{
    KineticStatus status = KineticClient_NoOp(&Fixture.session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
