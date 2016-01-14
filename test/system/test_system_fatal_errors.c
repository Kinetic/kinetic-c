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
#include "kinetic_client.h"
#include "kinetic_admin_client.h"

KineticClient * client;
KineticSession * session;
const char hmacKey[] = "bad_key"; // Purposely bad hmac key to cause HMAC validation faliure on device end

void setUp(void)
{
    SystemTestSetupWithIdentity(3, 1, (const uint8_t*)hmacKey, strlen(hmacKey));
}

void tearDown(void)
{
    /* Don't shut down here, since we're testing sessions being terminated. */
    if (SystemTestIsUnderSimulator()) {
        SystemTestShutDown();
    }
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

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SESSION_TERMINATED, status);
    status = KineticClient_DestroySession(Fixture.session);
    if (status != KINETIC_STATUS_CONNECTION_ERROR) {
        TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying client!");
    }
    status = KineticAdminClient_DestroySession(Fixture.adminSession);
    TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when destroying admin client!");
    KineticClient_Shutdown(Fixture.client);
}
