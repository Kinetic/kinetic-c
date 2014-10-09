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

// uint8_t hmacKeyBuffer[KINETIC_MAX_KEY_LEN];
uint8_t data[PDU_VALUE_MAX_LEN];

void SystemTestSetup(SystemTestFixture* fixture)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");

    ByteArray hmacArray = ByteArray_CreateWithCString("asdfasdf");
    if (!fixture->connected) {
        *fixture = (SystemTestFixture) {
            .config = (KineticSession) {
                .host = "localhost",
                .port = KINETIC_PORT,
                .clusterVersion = 0,
                .identity =  1,
                .nonBlocking = false,
                .hmacKey = hmacArray,
            },
            .keyToDelete = BYTE_BUFFER_NONE,
            .connected = fixture->connected,
            .testIgnored = false,
        };
        KineticStatus status = KineticClient_Connect(
                                   &fixture->config, &fixture->handle);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        fixture->expectedSequence = 0;
        fixture->connected = true;
        fixture->keyToDelete = ByteBuffer_Create(fixture->keyData, sizeof(fixture->keyData), 0);
    }
    else {
        fixture->testIgnored = false;
    }

    // TEST_ASSERT_EQUAL_MESSAGE(
    //     fixture->expectedSequence,
    //     fixture->connection.sequence,
    //     "Failed validating starting sequence count for the"
    //     " operation w/session!");
}

void SystemTestEraseSimulator(SystemTestFixture* fixture)
{ LOG_LOCATION;
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");
    if (fixture->keyToDelete.bytesUsed > 0) {
        char verData[] = "v1.0";
        ByteBuffer ver = ByteBuffer_Create((uint8_t*)verData, strlen(verData), strlen(verData));
        KineticEntry entry = {
            .key = fixture->keyToDelete,
            .dbVersion = ver,
        };
        KineticStatus status = KineticClient_Delete(fixture->handle, &entry);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG("Failed to delete object via specified key!");
        }
        else {
            LOG("DELETED!");
        }

        verData[1] = '2';
        entry = (KineticEntry) {
            .key = fixture->keyToDelete,
            .dbVersion = ver,
        };
        status = KineticClient_Delete(fixture->handle, &entry);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG("Failed to delete object via specified key!");
        }
        else {
            LOG("NOTHING DELETED!");
        }

        verData[1] = '3';
        entry = (KineticEntry) {
            .key = fixture->keyToDelete,
            .dbVersion = ver,
        };
        status = KineticClient_Delete(fixture->handle, &entry);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG("Failed to delete object via specified key!");
        }
        else {
            LOG("NOTHING DELETED!");
        }
    }
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
}
