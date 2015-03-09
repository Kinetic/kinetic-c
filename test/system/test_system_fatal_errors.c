/*
* kinetic-c
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
#include "kinetic_client.h"

KineticClient * client;
KineticSession * session;
const char hmacKey[] = "bad_key"; // Purposely bad hmac key to cause HMAC validation faliure on device end

void setUp(void)
{
    SystemTestSetupWithIdentity(3, 1, (const uint8_t*)hmacKey, strlen(hmacKey));
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_Put_with_invalid_HMAC_should_cause_hangup(void)
{
    if (SystemTestIsUnderSimulator()) {
        TEST_IGNORE_MESSAGE("Simulator does not yet support remote hangup on HMAC error (as of 2015-03-09)")
    }

    uint8_t KeyData[32], TagData[32], ValueData[32];

    KineticEntry putEntry = {
        .key = ByteBuffer_CreateAndAppendCString(KeyData, sizeof(KeyData), "some_key"),
        .tag = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "some_tag"),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_CreateAndAppendCString(ValueData, sizeof(ValueData), "some_value"),
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINTEIC_STATUS_SESSION_TERMINATED, status);
}
