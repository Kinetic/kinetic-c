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
    KineticSessionConfig config = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_PORT,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = SESSION_IDENTITY,
        .hmacKey = ByteArray_Create(config.keyData, strlen(SESSION_HMAC_KEY)),
    };
    strcpy((char*)config.keyData, SESSION_HMAC_KEY);

    KineticSessionConfig adminConfig = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_TLS_PORT,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .hmacKey = ByteArray_Create(config.keyData, strlen(SESSION_HMAC_KEY)),
        .pin = ByteArray_Create(adminConfig.pinData, strlen(SESSION_PIN)),
        .useSsl = true,
    };
    strcpy((char*)config.keyData, SESSION_HMAC_KEY);
    strcpy((char*)adminConfig.pinData, SESSION_PIN);

    Fixture = (SystemTestFixture) {
        .session = (KineticSession) {.config = config},
        .adminSession = (KineticSession) {.config = adminConfig},
        .connected = false,
        .client = KineticClient_Init("stdout", log_level),
    };

    KineticStatus status = KineticClient_CreateConnection(&Fixture.session, Fixture.client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    status = KineticAdminClient_CreateConnection(&Fixture.adminSession, Fixture.client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    Fixture.connected = true;
}

void SystemTestShutDown(void)
{
    if (Fixture.connected) {
        KineticStatus status = KineticClient_DestroyConnection(&Fixture.session);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying client!");
        status = KineticAdminClient_DestroyConnection(&Fixture.adminSession);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying admin client!");
        KineticClient_Shutdown(Fixture.client);
        memset(&Fixture, 0, sizeof(Fixture));
    }
}

bool SystemTestIsUnderSimulator(void)
{
    return (0 == strncmp(SYSTEM_TEST_HOST, "localhost", strlen("localhost")));
}
