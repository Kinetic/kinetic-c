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

#include "unity.h"
#include "unity_helper.h"
#include <protobuf-c/protobuf-c.h>
#include "kinetic_connection.h"
#include "kinetic_pdu.h"
#include "kinetic_exchange.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"
#include "kinetic_socket.h"
#include "kinetic_logger.h"

#include <string.h>

KineticPDU PDU;
KineticConnection Connection;
KineticExchange Exchange;
KineticMessage Message;
int64_t ProtoBufferLength;
uint8_t Value[1024*1024];
const int64_t ValueLength = (int64_t)sizeof(Value);

void setUp(void)
{
    uint32_t i;

    // Assemble a Kinetic protocol instance
    Connection = KineticConnection_Init();
    KineticExchange_Init(&Exchange, 0x7766554433221100, 5544332211, &Connection);
    KineticExchange_SetClusterVersion(&Exchange, 0x0011223344556677);
    KineticMessage_Init(&Message);

    // Compute the size of the encoded proto buffer
    ProtoBufferLength = KineticProto_get_packed_size(&Message.proto);

    // Populate the value buffer with test payload
    for (i = 0; i < ValueLength; i++)
    {
        Value[i] = (uint8_t)(i & 0xFFu);
    }
}

void tearDown(void)
{
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer(void)
{
    KineticPDU_Init(&PDU, &Exchange, &Message, NULL, 0);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Exchange, PDU.exchange);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&Message, PDU.protobuf);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', PDU.header.versionPrefix);

    // Validate protobuf length
    ProtoBufferLength = KineticProto_get_packed_size(&Message.proto);
    TEST_ASSERT_EQUAL_INT32(ProtoBufferLength, PDU.protobufLength);
    TEST_ASSERT_EQUAL_NBO_INT32(ProtoBufferLength, PDU.header.protobufLength);

    // Validate 'value' field is empty
    TEST_ASSERT_EQUAL_INT32(0, PDU.valueLength);
    TEST_ASSERT_EQUAL_NBO_INT32(0, PDU.header.valueLength);
    TEST_ASSERT_NULL(PDU.value);
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer_and_value_payload(void)
{
    KineticPDU_Init(&PDU, &Exchange, &Message, Value, ValueLength);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Exchange, PDU.exchange);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&Message, PDU.protobuf);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', PDU.header.versionPrefix);

    // Validate protobuf length
    ProtoBufferLength = KineticProto_get_packed_size(&Message.proto);
    TEST_ASSERT_EQUAL_INT32(ProtoBufferLength, PDU.protobufLength);
    TEST_ASSERT_EQUAL_NBO_INT32(ProtoBufferLength, PDU.header.protobufLength);

    // Validate value field size and content association
    TEST_ASSERT_EQUAL_INT32(ValueLength, PDU.valueLength);
    TEST_ASSERT_EQUAL_NBO_INT32(ValueLength, PDU.header.valueLength);
    TEST_ASSERT_EQUAL_PTR(Value, PDU.value);
}
