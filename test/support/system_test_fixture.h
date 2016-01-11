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

void SystemTestSetup(int log_level, bool secure_erase);
void SystemTestSetupWithIdentity(int log_level, int64_t identity,
    const uint8_t *key, size_t key_size);
void SystemTestShutDown(void);
bool SystemTestIsUnderSimulator(void);
char* GetSystemTestHost1(void);
int GetSystemTestPort1(void);
int GetSystemTestTlsPort1(void);
char* GetSystemTestHost2(void);
int GetSystemTestPort2(void);
int GetSystemTestTlsPort2(void);

#endif // _SYSTEM_TEST_FIXTURE
