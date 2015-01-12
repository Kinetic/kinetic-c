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
#include "kinetic_logger.h"

uint8_t data[KINETIC_OBJ_SIZE];
KineticClient * client;

void SystemTestSetup(SystemTestFixture* fixture, int log_level)
{
    client = KineticClient_Init("stdout", log_level);

    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");

    memset(fixture, 0, sizeof(SystemTestFixture));

    KineticStatus status;
    ByteArray hmacArray = ByteArray_CreateWithCString("asdfasdf");
    if (!fixture->connected) {
        *fixture = (SystemTestFixture) {
            .session = (KineticSession) {
                .config = (KineticSessionConfig) {
                    .host = SYSTEM_TEST_HOST,
                    .port = KINETIC_PORT,
                    .clusterVersion = 0,
                    .identity = 1,
                    .hmacKey = hmacArray,
                },
            },
            .connected = fixture->connected,
            .testIgnored = false,
        };
        status = KineticClient_CreateConnection(&fixture->session, client);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        fixture->expectedSequence = 0;
        fixture->connected = true;
    }
    else {
        fixture->testIgnored = false;
    }

    // Erase the drive
    // status = KineticClient_InstantSecureErase(&fixture->session);
    // TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // TEST_ASSERT_EQUAL_MESSAGE(
    //     fixture->expectedSequence,
    //     fixture->connection.sequence,
    //     "Failed validating starting sequence count for the"
    //     " operation w/session!");
}

void SystemTestTearDown(SystemTestFixture* fixture)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");

    if (!fixture->testIgnored) {
        fixture->expectedSequence++;
        // TEST_ASSERT_EQUAL_MESSAGE(
        //     fixture->expectedSequence,
        //     fixture->connection.sequence,
        //     "Sequence should post-increment for every operation on the session!");
    }

    KineticStatus status = KineticClient_DestroyConnection(&fixture->session);
    TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when disconnecting client!");

    KineticClient_Shutdown(client);
}

bool SystemTestIsUnderSimulator(void)
{
    return 0 == strncmp(SYSTEM_TEST_HOST, "localhost", strlen("localhost"));
}
