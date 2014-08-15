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
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"

#include "unity.h"
#include "unity_helper.h"
#include "protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

static const int64_t clusterVersion = 0;
static const int64_t identity = 1;
static const char* hmacKey = "asdfasdf";
static char* version = "v1.0";
static char* valueKey = "my_key_3.1415927";
static char* tag = "SomeTagValue";

static KineticExchange exchange;
static KineticOperation operation;
static KineticPDU request, response;
static KineticConnection connection;
static KineticMessage requestMsg, responseMsg;
static bool success;
static bool connected = false;
static bool CloseConnection = false;
static uint8_t value[1024*1024];
static int64_t valueLength = sizeof(value);

void setUp(void)
{
    int i;
    for (i = 0; i < valueLength; i++)
    {
        value[i] = (uint8_t)(0x0ff & i);
    }

    if (!connected)
    {
        KineticClient_Init(NULL);

        success = KineticClient_Connect(&connection, "localhost", KINETIC_PORT, true);

        TEST_ASSERT_TRUE(success);
        TEST_ASSERT(connection.socketDescriptor >= 0);

        success = KineticClient_ConfigureExchange(&exchange, &connection, clusterVersion, identity, hmacKey, strlen(hmacKey));
        TEST_ASSERT_TRUE(success);

        connected = true;
    }

    operation = KineticClient_CreateOperation(&exchange, &request, &requestMsg, &response);
}

void tearDown(void)
{
}

// -----------------------------------------------------------------------------
// Put Command - Write a blob of data to a Kinetic Device
//
// Inspected Request: (m/d/y)
// -----------------------------------------------------------------------------
//
//  TBD!
//
void test_Put_should_create_new_object_on_device(void)
{
    KineticProto_Status_StatusCode status =
        KineticClient_Put(&operation, version, valueKey, NULL, tag, value, valueLength);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, exchange.sequence);
}

void test_Put_should_update_object_data_on_device(void)
{
    KineticProto_Status_StatusCode status =
        KineticClient_Put(&operation, NULL, valueKey, version, tag, value, valueLength);

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(1, exchange.sequence);
}

void test_Put_should_update_object_data_on_device_and_update_version(void)
{
    KineticProto_Status_StatusCode status =
        KineticClient_Put(&operation, "v2.0", valueKey, version, tag, value, valueLength);

    TEST_ASSERT_EQUAL(2, exchange.sequence);

    TEST_IGNORE_MESSAGE("Java simulator is responding with VERSION_MISMATCH(8), "
                        "but request should be valid and update dbVersion!");

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
}

void test_Put_Suite_TearDown(void)
{
    if (connected)
    {
        KineticClient_Disconnect(&connection);
    }
    connected = false;
}
