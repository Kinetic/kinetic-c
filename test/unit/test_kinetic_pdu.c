/*
 * kinetic-c-client
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
#include "kinetic_pdu.h"
#include "kinetic_exchange.h"
#include "kinetic_proto.h"
#include <string.h>

KineticProto Proto;
KineticProto_Command Command;
KineticProto_Header Header;
int64_t ProtoBufferLength;
uint8_t Value[1024*1024];
const int64_t ValueLength = (int64_t)sizeof(Value);
uint8_t PDUBuffer[PDU_MAX_LEN];
KineticPDU PDU;

void setUp(void)
{
    uint32_t i;

    // Assemble a Kinetic protocol instance
    KineticProto_init(&Proto);
    KineticProto_command_init(&Command);
    KineticProto_header_init(&Header);
    Header.clusterversion = 0x0011223344556677;
    Header.has_clusterversion = true;
    Header.identity = 0x7766554433221100;
    Header.has_identity = true;
    Command.header = &Header;
    Proto.command = &Command;

    // Compute the size of the encoded proto buffer
    ProtoBufferLength = KineticProto_get_packed_size(&Proto);

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

void test_KineticPDU_Create_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer(void)
{
    int i;

    KineticPDU_Create(&PDU, PDUBuffer, &Proto, NULL, 0);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', *PDU.prefix);

    // Validate proto buf size and packed content
    ProtoBufferLength = KineticProto_get_packed_size(&Proto);
    TEST_ASSERT_EQUAL_NBO_INT64(ProtoBufferLength, PDU.protoLength);

    // Validate value field size (no content)
    TEST_ASSERT_EQUAL_NBO_INT64(0, PDU.valueLength);
}

void test_KineticPDU_Create_should_populate_the_PDU_structure_and_PDU_buffer_with_the_supplied_protocol_buffer_and_value_payload(void)
{
    int i;

    KineticPDU_Create(&PDU, PDUBuffer, &Proto, Value, ValueLength);

    // Valiate prefix
    TEST_ASSERT_EQUAL_HEX8('F', *PDU.prefix);

    // Validate proto buf size and packed content
    ProtoBufferLength = KineticProto_get_packed_size(&Proto);
    TEST_ASSERT_EQUAL_NBO_INT64(ProtoBufferLength, PDU.protoLength);

    // Validate value field size and content
    TEST_ASSERT_EQUAL_NBO_INT64(ValueLength, PDU.valueLength);
    for (i = 0; i < ValueLength; i++)
    {
        char err[64];
        sprintf(err, "@ value payload index %d", i);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((i & 0xFFu), PDU.value[i], err);
    }
}
