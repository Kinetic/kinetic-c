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

#include "system_test_fixture.h"
#include "unity_helper.h"
#include "kinetic_client.h"
#include "kinetic_admin_client.h"

SystemTestFixture Fixture = {.connected = false};

void SystemTestSetup(int log_level)
{
    const uint8_t *key = (const uint8_t *)SESSION_HMAC_KEY;
    SystemTestSetupWithIdentity(log_level, SESSION_IDENTITY, key, strlen((const char*)key));
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
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_PORT,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = identity,
        .hmacKey = ByteArray_Create((void *)key, key_size),
    };
    strcpy((char*)config.keyData, SESSION_HMAC_KEY);
    KineticStatus status = KineticClient_CreateSession(&config, Fixture.client, &Fixture.session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    KineticSessionConfig adminConfig = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_TLS_PORT,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = identity,
        .hmacKey = ByteArray_Create((void *)key, key_size),
        .useSsl = true,
    };
    strcpy((char*)config.keyData, SESSION_HMAC_KEY);
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
    return (0 == strncmp(SYSTEM_TEST_HOST, "localhost", strlen("localhost")));
}
