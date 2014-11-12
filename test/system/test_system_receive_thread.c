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
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_device_info.h"
#include "kinetic_serial_allocator.h"
#include "kinetic_proto.h"
#include "kinetic_allocator.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

static SystemTestFixture Fixture;

void setUp(void)
{
    SystemTestSetup(&Fixture);
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_KineticClient_should_process_initial_unsolicited_status_response(void)
{
    int secondsWaiting = 0, maxWaiting = 2;
    KineticConnection* connection = KineticConnection_FromHandle(Fixture.handle);
    while(connection->connectionID == 0) {
        LOG0("Waiting for connection ID...");
        sleep(1);
        secondsWaiting++;
        TEST_ASSERT_TRUE_MESSAGE(secondsWaiting < maxWaiting,
            "Timed out waiting for initial unsolicited status!");
    }
    TEST_ASSERT_TRUE_MESSAGE(connection->connectionID > 0, "Invalid connection ID!");
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
