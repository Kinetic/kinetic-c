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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
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

#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"
#include <string.h>
#include <stdlib.h>

static SystemTestFixture Fixture = {
    .config = (KineticSession) {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity =  1,
        .nonBlocking = false,
        .hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING("asdfasdf"),
    }
};

void setUp(void)
{
    SystemTestSetup(&Fixture);
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
void test_Delete_should_delete_an_object_from_device(void)
{
    KineticStatus status;

    // Create an object so that we have something to delete
    KineticKeyValue putMetadata = {
        .key = BYTE_ARRAY_INIT_FROM_CSTRING("DELETE test key"),
        .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue"),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah blah blah... etc."),
    };
    status = KineticClient_Put(Fixture.handle, &putMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    // Validate the object exists initially
    KineticKeyValue getMetadata = {
        .key = BYTE_ARRAY_INIT_FROM_CSTRING("DELETE test key"),
    };
    status = KineticClient_Get(Fixture.handle, &getMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    // Delete the object
    KineticKeyValue metadata = {
        .key = BYTE_ARRAY_INIT_FROM_CSTRING("DELETE test key"),
        .tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue"),
        .dbVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
        .algorithm = KINETIC_ALGORITHM_SHA1,
    };
    status = KineticClient_Delete(Fixture.handle, &metadata);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, metadata.value.len);

    // Validate the object no longer exists
    status = KineticClient_Get(Fixture.handle, &getMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_DATA_ERROR, status);
    TEST_ASSERT_EQUAL(0, getMetadata.value.len);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
