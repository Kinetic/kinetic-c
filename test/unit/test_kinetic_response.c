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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_bus.h"
#include "kinetic_response.h"
#include "kinetic_nbo.h"
#include "kinetic_proto.h"
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
    KineticProto_Command Command;
    memset(&Command, 0, sizeof(Command));
    KineticProto_Command_KeyValue* keyValue = NULL;

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


void test_KineticResponse_GetKeyRange_should_return_the_KineticProto_Command_Range_from_the_message_if_avaliable(void)
{
    KineticProto_Command_Range* range = NULL;

    range = KineticResponse_GetKeyRange(NULL);
    TEST_ASSERT_NULL(range);

    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);
    
    KineticProto_Message Message;
    memset(&Message, 0, sizeof(Message));
    Response.proto = &Message;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);

    KineticProto_Command Command;
    memset(&Command, 0, sizeof(Command));
    Response.command = &Command;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_NULL(range);

    KineticProto_Command_Body Body;
    memset(&Body, 0, sizeof(Body));
    Response.command->body = &Body;

    KineticProto_Command_Range Range;
    memset(&Range, 0, sizeof(Range));
    Body.range = &Range;
    range = KineticResponse_GetKeyRange(&Response);
    TEST_ASSERT_EQUAL_PTR(&Range, range);
}
