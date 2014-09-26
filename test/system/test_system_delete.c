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

static SystemTestFixture Fixture = {
    .host = "localhost",
    .port = KINETIC_PORT,
    .clusterVersion = 0,
    .identity =  1,
};
static ByteArray Version;
static ByteArray ValueKey;
static ByteArray Tag;
static ByteArray TestValue;
static KineticProto_Algorithm Algorithm = KINETIC_PROTO_ALGORITHM_SHA1;

void setUp(void)
{
    SystemTestSetup(&Fixture);

    Version = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    ValueKey = BYTE_ARRAY_INIT_FROM_CSTRING("DELETE system test blob");
    Tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeOTagValue");
    TestValue = BYTE_ARRAY_INIT_FROM_CSTRING("lorem ipsum... blah blah blah... etc.");

    KineticKeyValue metadata = {
        .key = ValueKey,
        .newVersion = Version,
        .tag = Tag,
        .algorithm = Algorithm,
        .value = TestValue,
    };

    KineticStatus status = KineticClient_Put(&Fixture.instance.operation, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);

    Fixture.expectedSequence++;
    TEST_ASSERT_EQUAL_MESSAGE(
        Fixture.expectedSequence,
        Fixture.connection.sequence,
        "Sequence should post-increment for every operation on the session!");
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
    KineticKeyValue getMetadata = {
        .key = ValueKey,
        .tag = Tag,
        .dbVersion = Version,
        .algorithm = Algorithm,
    };
    KineticStatus status;

    // Validate the object exists initially
    Fixture.instance.operation = KineticClient_CreateOperation(
                                     &Fixture.connection, &Fixture.request, &Fixture.response);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Fixture.request, &Fixture.connection);
    KINETIC_PDU_INIT(&Fixture.response, &Fixture.connection);
    status = KineticClient_Get(&Fixture.instance.operation, &getMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteArray(TestValue, getMetadata.value);
    Fixture.expectedSequence++;
    TEST_ASSERT_EQUAL_MESSAGE(Fixture.expectedSequence, Fixture.connection.sequence,
                              "Sequence should post-increment for every operation on the session!");

    // Delete the object
    KineticKeyValue metadata = {
        .key = ValueKey,
        .dbVersion = Version,
    };
    metadata.algorithm = (KineticProto_Algorithm)0;
    metadata.tag = BYTE_ARRAY_NONE;
    Fixture.instance.operation = KineticClient_CreateOperation(
                                     &Fixture.connection, &Fixture.request, &Fixture.response);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Fixture.request, &Fixture.connection);
    KINETIC_PDU_INIT(&Fixture.response, &Fixture.connection);
    status = KineticClient_Delete(&Fixture.instance.operation, &metadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, metadata.value.len);
    Fixture.expectedSequence++;
    TEST_ASSERT_EQUAL_MESSAGE(Fixture.expectedSequence, Fixture.connection.sequence,
                              "Sequence should post-increment for every operation on the session!");

    // Validate the object no longer exists
    Fixture.instance.operation = KineticClient_CreateOperation(
                                     &Fixture.connection, &Fixture.request, &Fixture.response);
    KINETIC_PDU_INIT_WITH_MESSAGE(&Fixture.request, &Fixture.connection);
    KINETIC_PDU_INIT(&Fixture.response, &Fixture.connection);
    status = KineticClient_Get(&Fixture.instance.operation, &getMetadata);
    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_STATUS_DATA_ERROR, status);
    TEST_ASSERT_EQUAL(0, getMetadata.value.len);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
