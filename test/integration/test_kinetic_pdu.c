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
uint8_t PDUBuffer[PDU_MAX_LEN];

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
    memset(PDUBuffer, 0, sizeof(PDUBuffer));

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
    KineticPDU_Init(&PDU, &Exchange, PDUBuffer, &Message, NULL, 0);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Exchange, PDU.exchange);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&Message, PDU.message);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', *PDU.prefix);

    // Validate 'value' field is empty
    TEST_ASSERT_EQUAL(0, PDU.valueLength);

    // Validate proto buf size and packed content
    ProtoBufferLength = KineticProto_get_packed_size(&Message.proto);
    TEST_ASSERT_EQUAL_NBO_INT64(ProtoBufferLength, PDU.protoLength);

    // Validate value field size (no content)
    TEST_ASSERT_NULL(PDU.value);
    TEST_ASSERT_NULL(PDU.valueLength);
}

void test_KineticPDU_Init_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer_and_value_payload(void)
{
    size_t i;

    KineticPDU_Init(&PDU, &Exchange, PDUBuffer, &Message, Value, ValueLength);

    // Validate KineticExchange associated
    TEST_ASSERT_EQUAL_PTR(&Exchange, PDU.exchange);

    // Validate KineticMessage associated
    TEST_ASSERT_EQUAL_PTR(&Message, PDU.message);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', *PDU.prefix);

    // Validate proto buf size and packed content
    ProtoBufferLength = KineticProto_get_packed_size(&Message.proto);
    TEST_ASSERT_EQUAL_NBO_INT64(ProtoBufferLength, PDU.protoLength);

    // Validate value field size and content
    TEST_ASSERT_EQUAL_NBO_INT64(ValueLength, PDU.valueLength);
    for (i = 0; i < ValueLength; i++)
    {
        char err[64];
        sprintf(err, "@ value payload index %zu", i);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((i & 0xFFu), PDU.value[i], err);
    }
}
