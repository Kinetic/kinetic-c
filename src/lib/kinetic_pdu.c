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

#include "kinetic_pdu.h"
#include <string.h>

static void KineticPDU_PackInt32(uint8_t* const buffer, int32_t value);
static int32_t KineticPDU_UnpackInt32(const uint8_t* const buffer);

void KineticPDU_Init(
    KineticPDU* const pdu,
    KineticExchange* const exchange,
    KineticMessage* const message,
    uint8_t* const value,
    int32_t valueLength)
{
    size_t protoPackedSize;

    assert(pdu != NULL);
    assert(message != NULL);
    assert(exchange != NULL);

    // Initialize the instance struct
    memset(pdu, 0, sizeof(KineticPDU));

    // Associate with the specified exchange
    pdu->exchange = exchange;

    // Associate with the specified message
    pdu->protobuf = message;

    // Configure the protobuf header with exchange info
    KineticExchange_ConfigureHeader(exchange, &pdu->protobuf->header);

    // Setup the PDU header fields
    pdu->header.versionPrefix = (uint8_t)'F';

    // Associate with protobuf
    pdu->protobufLength = KineticProto_get_packed_size(&pdu->protobuf->proto);
    KineticPDU_PackInt32((uint8_t*)&pdu->header.protobufLength, pdu->protobufLength); // Pack protobuf length field

    // Associate with payload (if present)
    pdu->valueLength = valueLength;
    KineticPDU_PackInt32((uint8_t*)&pdu->header.valueLength, pdu->valueLength); // Pack value length field
    pdu->value = value;
}

void KineticPDU_PackInt32(uint8_t* const buffer, int32_t value)
{
    int i;
    for (i = sizeof(int32_t) - 1; i >= 0; i--)
    {
        buffer[i] = value & 0xFF;
        value = value >> 8;
    }
}

int32_t KineticPDU_UnpackInt32(const uint8_t* const buffer)
{
    int i;
    int32_t value = 0;
    for (i = 0; i < sizeof(int32_t); i++)
    {
        value <<= 8;
        value += buffer[i];
    }
    return value;
}
