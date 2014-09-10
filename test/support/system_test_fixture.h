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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#ifndef _SYSTEM_TEST_FIXTURE
#define _SYSTEM_TEST_FIXTURE

#include "kinetic_types.h"

typedef struct _SystemTestInstance
{
    bool testIgnored;
    KineticOperation operation;
} SystemTestInstance;

typedef struct _SystemTestFixture
{
    char host[HOST_NAME_MAX];
    int port;
    int64_t clusterVersion;
    int64_t identity;
    bool nonBlocking;
    int64_t expectedSequence;
    ByteArray hmacKey;
    bool connected;
    KineticConnection connection;
    KineticPDU request;
    KineticPDU response;
    SystemTestInstance instance;
} SystemTestFixture;

void SystemTestSetup(SystemTestFixture* fixture);
void SystemTestTearDown(SystemTestFixture* fixture);
void SystemTestSuiteTearDown(SystemTestFixture* fixture);

#define SYSTEM_TEST_SUITE_TEARDOWN(_fixture) \
void test_Suite_TearDown(void) \
{ \
    /*TEST_ASSERT_NOT_NULL_MESSAGE((_fixture), "System test fixture passed to 'SYSTEM_TEST_SUITE_TEARDOWN' is NULL!");*/ \
    if ((_fixture)->connected) \
        KineticClient_Disconnect(&(_fixture)->connection); \
    (_fixture)->connected = false; \
    (_fixture)->instance.testIgnored = true; \
}

#endif // _SYSTEM_TEST_FIXTURE
