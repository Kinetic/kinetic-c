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

uint8_t data[KINETIC_OBJ_SIZE];
KineticClient * client;

void SystemTestSetup(SystemTestFixture* fixture, int log_level)
{
    LOG_LOCATION;
    client = KineticClient_Init("stdout", log_level);

    memset(fixture, 0, sizeof(SystemTestFixture));

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
        .port = KINETIC_PORT,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .pin = ByteArray_Create(adminConfig.pinData, strlen(SESSION_PIN)),
        .useSsl = true,
    };
    strcpy((char*)adminConfig.pinData, SESSION_PIN);

    if (!fixture->connected) {
        *fixture = (SystemTestFixture) {
            .session = (KineticSession) {.config = config},
            .adminSession = (KineticSession) {.config = adminConfig},
            .connected = fixture->connected,
        };
        KineticStatus status = KineticClient_CreateConnection(&fixture->session, client);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        status = KineticAdminClient_CreateConnection(&fixture->adminSession, client);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        
        fixture->connected = true;
    }

    // Erase the drive
    // status = KineticAdminClient_InstantSecureErase(&fixture->adminSession);
    // TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    LOG_LOCATION;
}

void SystemTestTearDown(SystemTestFixture* fixture)
{
    LOG_LOCATION;
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");

    if (fixture->connected) {
        KineticStatus status = KineticClient_DestroyConnection(&fixture->session);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying client!");
        status = KineticAdminClient_DestroyConnection(&fixture->adminSession);
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying admin client!");
        KineticClient_Shutdown(client);
    }
    LOG_LOCATION;
}

bool SystemTestIsUnderSimulator(void)
{
    return (0 == strncmp(SYSTEM_TEST_HOST, "localhost", strlen("localhost")));
}
