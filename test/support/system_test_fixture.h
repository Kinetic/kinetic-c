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
    KineticSession* session;
    KineticSession* adminSession;
    bool connected;
    int64_t expectedSequence;
    KineticClient * client;
    char host1[257];
    int port1;
    int tlsPort1;
    char host2[257];
    int port2;
    int tlsPort2;
    bool configurationLoaded;
} SystemTestFixture;

extern SystemTestFixture Fixture;

void SystemTestSetup(int log_level);
void SystemTestSetupWithIdentity(int log_level, int64_t identity,
    const uint8_t *key, size_t key_size);
void SystemTestShutDown(void);
bool SystemTestIsUnderSimulator(void);
const char* GetSystemTestHost1(void);
int GetSystemTestPort1(void);
int GetSystemTestTlsPort1(void);
const char* GetSystemTestHost2(void);
int GetSystemTestPort2(void);
int GetSystemTestTlsPort2(void);

#define SYSTEM_TEST_SUITE_TEARDOWN void test_Suite_TearDown(void) {SystemTestShutDown();}

#endif // _SYSTEM_TEST_FIXTURE
