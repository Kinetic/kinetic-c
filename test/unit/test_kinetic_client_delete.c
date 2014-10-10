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
#include "kinetic_operation.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_pdu.h"
#include <stdio.h>
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;
static KineticConnection Connection;
static const int64_t ClusterVersion = 1234;
static const int64_t Identity = 47;
static ByteArray HmacKey;
static KineticSessionHandle DummyHandle = 1;
static KineticSessionHandle SessionHandle = KINETIC_HANDLE_INVALID;
KineticPDU Request, Response;


void setUp(void)
{
    KINETIC_CONNECTION_INIT(&Connection);
    Connection.connected = false; // Ensure gets set appropriately by internal connect call
    HmacKey = ByteArray_CreateWithCString("some hmac key");
    KINETIC_SESSION_INIT(&Session, "somehost.com", ClusterVersion, Identity, HmacKey);

    KineticConnection_NewConnection_ExpectAndReturn(&Session, DummyHandle);
    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticConnection_Connect_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);
    KineticConnection_ReceiveDeviceStatusMessage_ExpectAndReturn(&Connection, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Connect(&Session, &SessionHandle);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(DummyHandle, SessionHandle);
}

void tearDown(void)
{
}

void test_KineticClient_Delete_should_execute_DELETE_operation(void)
{
    ByteArray key = ByteArray_CreateWithCString("some_key");
    ByteArray tag = ByteArray_CreateWithCString("SomeTagValue");
    KineticEntry entry = {
        .key = ByteBuffer_CreateWithArray(key),
        .tag = ByteBuffer_CreateWithArray(tag),
    };

    KineticConnection_FromHandle_ExpectAndReturn(DummyHandle, &Connection);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Request);
    KineticAllocator_NewPDU_ExpectAndReturn(&Connection.pdus, &Connection, &Response);
    KineticPDU_Init_Expect(&Request, &Connection);
    KineticPDU_Init_Expect(&Response, &Connection);
    KineticConnection_IncrementSequence_Expect(&Connection);
    KineticMessage_ConfigureKeyValue_Expect(&Request.protoData.message, &entry);
    KineticPDU_Send_ExpectAndReturn(&Request, KINETIC_STATUS_SUCCESS);
    KineticPDU_Receive_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticPDU_GetStatus_ExpectAndReturn(&Response, KINETIC_STATUS_SUCCESS);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Request);
    KineticAllocator_FreePDU_Expect(&Connection.pdus, &Response);

    KineticStatus status = KineticClient_Delete(DummyHandle, &entry);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
