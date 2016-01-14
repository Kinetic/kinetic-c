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
#include "kinetic_admin_client.h"

char OldPinData[8];
char NewPinData[8];
ByteArray OldPin, NewPin;
bool ErasePinSet, LockPinSet;

void setUp(void)
{
    SystemTestSetup(1, false);
    ErasePinSet = false;
    LockPinSet = false;
    strcpy(NewPinData, SESSION_PIN);
    OldPin = ByteArray_Create(OldPinData, 0);
    NewPin = ByteArray_Create(NewPinData, strlen(NewPinData));
}

void tearDown(void)
{
    if (ErasePinSet) {
        KineticStatus status = KineticAdminClient_SetErasePin(Fixture.adminSession,
            NewPin, OldPin);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    if (LockPinSet) {
        KineticStatus status = KineticAdminClient_SetLockPin(Fixture.adminSession,
            NewPin, OldPin);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    SystemTestShutDown();
}

void test_SetErasePin_should_succeed(void)
{
    KineticStatus status = KineticAdminClient_SetErasePin(Fixture.adminSession,
        OldPin, NewPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    ErasePinSet = (status == KINETIC_STATUS_SUCCESS);
}

void test_SetLockPin_should_succeed(void)
{
    KineticStatus status = KineticAdminClient_SetLockPin(Fixture.adminSession,
        OldPin, NewPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    LockPinSet = (status == KINETIC_STATUS_SUCCESS);
}
