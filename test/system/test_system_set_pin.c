/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
