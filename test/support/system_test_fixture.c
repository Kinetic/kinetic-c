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
#include "unity_helper.h"
#include "kinetic_client.h"
#include "kinetic_admin_client.h"

SystemTestFixture Fixture = {.connected = false};
static char InitPinData[8];
static ByteArray InitPin;

static void LoadConfiguration(void)
{
    if (!Fixture.configurationLoaded) {

        //----------------------------------------------------------------------
        // Configure test device #1

        // Configure host
        char * host = getenv("KINETIC_HOST");
        if (host == NULL) {
            host = getenv("KINETIC_HOST1");
        }
        if (host == NULL) {
            strncpy(Fixture.host1, "127.0.0.1", sizeof(Fixture.host1)-1);
        }
        else {
            strncpy(Fixture.host1, host, sizeof(Fixture.host1)-1);
        }

        // Configure standard port
        char * portString = getenv("KINETIC_PORT");
        if (portString == NULL) {
            portString = getenv("KINETIC_PORT1");
        }
        if (portString == NULL) {
            Fixture.port1 = KINETIC_PORT;
        }
        else {
            errno = 0;
            long port = strtol(portString, NULL, 0);
            if (port == 0 && errno != 0) {
                TEST_ASSERT_EQUAL_MESSAGE(0, errno, "Failed parsing kinetic device port! (errno != 0)");
            }
            Fixture.port1 = (int)port;
        }

        // Configure TLS port
        char * tlsPortString = getenv("KINETIC_TLS_PORT");
        if (tlsPortString == NULL) {
            tlsPortString = getenv("KINETIC_TLS_PORT1");
        }
        if (tlsPortString == NULL) {
            Fixture.tlsPort1 = KINETIC_TLS_PORT;
        }
        else {
            errno = 0;
            long port = strtol(tlsPortString, NULL, 0);
            if (port == 0 && errno != 0) {
                TEST_ASSERT_EQUAL_MESSAGE(0, errno, "Failed parsing kinetic device TLS port! (errno != 0)");
            }
            Fixture.tlsPort1 = (int)port;
        }

        //----------------------------------------------------------------------
        // Configure test device #2

        // Configure host
        host = getenv("KINETIC_HOST2");
        if (host == NULL) {
            strncpy(Fixture.host2, "127.0.0.1", sizeof(Fixture.host2)-1);
        }
        else {
            strncpy(Fixture.host2, host, sizeof(Fixture.host2)-1);
        }

        // Configure standard port
        portString = getenv("KINETIC_PORT2");
        if (portString == NULL) {
            Fixture.port2 = KINETIC_PORT+1;
        }
        else {
            errno = 0;
            long port = strtol(portString, NULL, 0);
            if (port == 0 && errno != 0) {
                TEST_ASSERT_EQUAL_MESSAGE(0, errno, "Failed parsing kinetic device port! (errno != 0)");
            }
            Fixture.port2 = (int)port;
        }

        // Configure TLS port
        tlsPortString = getenv("KINETIC_TLS_PORT2");
        if (tlsPortString == NULL) {
            Fixture.tlsPort2 = KINETIC_TLS_PORT+1;
        }
        else {
            errno = 0;
            long port = strtol(tlsPortString, NULL, 0);
            if (port == 0 && errno != 0) {
                TEST_ASSERT_EQUAL_MESSAGE(0, errno, "Failed parsing kinetic device TLS port! (errno != 0)");
            }
            Fixture.tlsPort2 = (int)port;
        }

        Fixture.configurationLoaded = true;
    }
}

char* GetSystemTestHost1(void)
{
    LoadConfiguration();
    return Fixture.host1;
}

int GetSystemTestPort1(void)
{
    LoadConfiguration();
    return Fixture.port1;
}

int GetSystemTestTlsPort1(void)
{
    LoadConfiguration();
    return Fixture.tlsPort1;
}

char* GetSystemTestHost2(void)
{
    LoadConfiguration();
    return Fixture.host2;
}

int GetSystemTestPort2(void)
{
    LoadConfiguration();
    return Fixture.port2;
}

int GetSystemTestTlsPort2(void)
{
    LoadConfiguration();
    return Fixture.tlsPort2;
}

void SystemTestSetup(int log_level, bool secure_erase)
{
    const uint8_t *key = (const uint8_t *)SESSION_HMAC_KEY;
    SystemTestSetupWithIdentity(log_level, SESSION_IDENTITY, key, strlen((const char*)key));

    if (secure_erase)
    {
        InitPin = ByteArray_Create(InitPinData, 0);
        KineticStatus status = KineticAdminClient_SecureErase(Fixture.adminSession, InitPin);
    	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
}

void SystemTestSetupWithIdentity(int log_level, int64_t identity,
    const uint8_t *key, size_t key_size)
{
    KineticClientConfig clientConfig = {
        .logFile = "stdout",
        .logLevel = log_level,
    };

    Fixture = (SystemTestFixture) {
        .connected = false,
        .client = KineticClient_Init(&clientConfig),
    };

    KineticSessionConfig config = {
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = identity,
        .hmacKey = ByteArray_Create((void *)key, key_size),
    };
    strncpy((char*)config.host, GetSystemTestHost1(), sizeof(config.host)-1);
    config.port = GetSystemTestPort1();
    strncpy((char*)config.keyData, SESSION_HMAC_KEY, sizeof(config.keyData)-1);
    KineticStatus status = KineticClient_CreateSession(&config, Fixture.client, &Fixture.session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    KineticSessionConfig adminConfig = {
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = identity,
        .hmacKey = ByteArray_Create((void *)key, key_size),
        .useSsl = true,
    };
    strncpy(adminConfig.host, GetSystemTestHost1(), sizeof(adminConfig.host)-1);
    adminConfig.port = GetSystemTestTlsPort1();
    strncpy((char*)adminConfig.keyData, SESSION_HMAC_KEY, sizeof(adminConfig.keyData)-1);
    status = KineticAdminClient_CreateSession(&adminConfig, Fixture.client, &Fixture.adminSession);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    Fixture.connected = true;
}

void SystemTestShutDown(void)
{
    if (Fixture.connected) {
        KineticStatus status = KineticClient_DestroySession(Fixture.session);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying client!");
        status = KineticAdminClient_DestroySession(Fixture.adminSession);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying admin client!");
        KineticClient_Shutdown(Fixture.client);
        memset(&Fixture, 0, sizeof(Fixture));
    }
}

bool SystemTestIsUnderSimulator(void)
{
    LoadConfiguration();
    return ((0 == strncmp(Fixture.host1, "localhost", strlen("localhost"))) ||
            (0 == strncmp(Fixture.host1, "127.0.0.1", strlen("127.0.0.1"))));
}
