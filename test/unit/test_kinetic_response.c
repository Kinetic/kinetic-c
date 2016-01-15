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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_bus.h"
#include "kinetic_response.h"
#include "kinetic_nbo.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_pdu_unpack.h"
#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

static uint32_t ClusterVersion = 7;
static KineticResponse Response;
static KineticSession Session;
static uint8_t ValueBuffer[KINETIC_OBJ_SIZE];
static ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

void setUp(void)
{
    KineticLogger_Init("stdout", 3);

    // Create and configure a new Kinetic protocol instance
;
    Session = (KineticSession) {
        .connected = true,
        .socket = 456,
        .config = (KineticSessionConfig) {
            .port = 1234,
            .host = "valid-host.com",
            .hmacKey = ByteArray_CreateWithCString("some valid HMAC key..."),
            .clusterVersion = ClusterVersion,
        }
    };
    ByteArray_FillWithDummyData(Value);
}

void tearDown(void)
{
    memset(&Response, 0, sizeof(Response));
    KineticLogger_Close();
}



void test_KineticResponse_GetKeyValue_should_return_NULL_if_message_has_no_KeyValue(void)
{
    Com__Seagate__Kinetic__Proto__Command Command;
    memset(&Command, 0, sizeof(Command));
    Com__Seagate__Kinetic__Proto__Command__KeyValue* keyValue = NULL;

    keyValue = KineticResponse_GetKeyValue(NULL);
    TEST_ASSERT_NULL(keyValue);

    Response.command = NULL;
    keyValue = KineticResponse_GetKeyValue(&Response);
    TEST_ASSERT_NULL(keyValue);

    Response.command = &Command;
    Command.body = NULL;
    keyValue = KineticResponse_GetKeyValue(&Response);
    TEST_ASSERT_NULL(keyValue);
}


void test_KineticResponse_GetKeyRange_should_return_the_Com__Seagate__Kinetic__Proto__Command__Range_from_the_message_if_avaliable(void)
{
    Com__Seagate__Kinetic__Proto__Command__Range* range = NULL;

    range = KineticResponse_GetKeyRange(NULL);
    TEST_ASSERT_NULL(range);

    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);

    Com__Seagate__Kinetic__Proto__Message Message;
    memset(&Message, 0, sizeof(Message));
    Response.proto = &Message;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);

    Com__Seagate__Kinetic__Proto__Command Command;
    memset(&Command, 0, sizeof(Command));
    Response.command = &Command;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);

    Com__Seagate__Kinetic__Proto__Command__Body Body;
    memset(&Body, 0, sizeof(Body));
    Response.command->body = &Body;

    Com__Seagate__Kinetic__Proto__Command__Range Range;
    memset(&Range, 0, sizeof(Range));
    Body.range = &Range;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_EQUAL_PTR(&Range, range);
}
