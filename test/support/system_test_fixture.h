/*
* kinetic-c-client
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

#ifndef _SYSTEM_TEST_FIXTURE
#define _SYSTEM_TEST_FIXTURE

#include "kinetic_types.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_logger.h"
#include "unity.h"
#include "unity_helper.h"

#ifndef SYSTEM_TEST_HOST
#define SYSTEM_TEST_HOST "localhost"
#endif

#ifndef SESSION_CLUSTER_VERSION
#define SESSION_CLUSTER_VERSION 0
#endif

#ifndef SESSION_HMAC_KEY
#define SESSION_HMAC_KEY "asdfasdf"
#endif

#ifndef SESSION_PIN
#define SESSION_PIN "1234"
#endif

#ifndef SESSION_IDENTITY
#define SESSION_IDENTITY 1
#endif

typedef struct _SystemTestFixture {
    KineticSession session;
    KineticSession adminSession;
    bool testIgnored;
    bool connected;
    int64_t expectedSequence;
} SystemTestFixture;

void SystemTestSetup(SystemTestFixture* fixture, int log_level);
void SystemTestTearDown(SystemTestFixture* fixture);
void SystemTestSuiteTearDown(SystemTestFixture* fixture);
bool SystemTestIsUnderSimulator(void);

#define SYSTEM_TEST_SUITE_TEARDOWN(_fixture) void test_Suite_TearDown(void) {SystemTestTearDown(_fixture);}

#endif // _SYSTEM_TEST_FIXTURE
