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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static ByteArray valueKey;
static ByteArray tag;
static ByteArray testValue;
static bool testDataWritten = false;
static SystemTestFixture Fixture = {
    .host = "localhost",
    .port = KINETIC_PORT,
    .clusterVersion = 0,
    .identity =  1,
};

void setUp(void)
{
    SystemTestSetup(&Fixture);
    valueKey = BYTE_ARRAY_INIT_FROM_CSTRING("GET system test blob");
    tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeOtherTagValue");
    testValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah blah blah... etc.");

    // Setup to write some test data
    Fixture.request.value = testValue;

    KineticKeyValue metadata = {
        .key = valueKey,
        .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .tag = tag,
        .algorithm = KINETIC_PROTO_ALGORITHM_SHA1,
        .value = testValue,
    };

    if (!testDataWritten) {
        KineticStatus status =
            KineticClient_Put(&Fixture.instance.operation,
                              &metadata);

        TEST_ASSERT_EQUAL_KINETIC_STATUS(
            KINETIC_STATUS_SUCCESS, status);

        Fixture.expectedSequence++;
        TEST_ASSERT_EQUAL_MESSAGE(
            Fixture.expectedSequence,
            Fixture.connection.sequence,
            "Sequence should post-increment for every operation on the session!");

        testDataWritten = true;
    }
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

// -----------------------------------------------------------------------------
// Put Command - Write a blob of data to a Kinetic Device
//
// Inspected Request: (m/d/y)
// -----------------------------------------------------------------------------
//
//  TBD!
//
void test_Get_should_retrieve_object_and_metadata_from_device(void)
{
    KineticKeyValue metadata = {.key = valueKey};
    ByteArray value = {.data = Fixture.response.valueBuffer, .len = testValue.len};

    KineticStatus status =
        KineticClient_Get(&Fixture.instance.operation,
                          &metadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(value, metadata.value);
}

void test_Get_should_retrieve_object_and_metadata_from_device_again(void)
{
    sleep(1);

    KineticKeyValue metadata = {.key = valueKey};
    ByteArray value = {.data = Fixture.response.valueBuffer, .len = testValue.len};

    KineticStatus status =
        KineticClient_Get(&Fixture.instance.operation,
                          &metadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(
        KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_ByteArray(value, metadata.value);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
