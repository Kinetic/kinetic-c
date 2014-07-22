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

static void KineticPDU_PackInt64(uint8_t* const buffer, int64_t value);
static int64_t KineticPDU_UnpackInt64(const uint8_t* const buffer);

void KineticPDU_Init(
    KineticPDU* const pdu,
    KineticExchange* const exchange,
    uint8_t* const buffer,
    KineticMessage* const message,
    const uint8_t* const value,
    int64_t valueLength)
{
    size_t protoPackedSize;

    assert(pdu != NULL);
    assert(message != NULL);
    assert(exchange != NULL);
    assert(buffer != NULL);

    // Initialize the instance struct
    memset(pdu, 0, sizeof(KineticPDU));

    // Associate with the specified exchange
    pdu->exchange = exchange;

    // Configure the protobuf header with exchange info
    KineticExchange_ConfigureHeader(exchange, &message->header);

    // Assign the data buffer pointer
    pdu->data = buffer;

    // Setup the PDU header fields
    pdu->prefix = &buffer[0];
    *pdu->prefix = (uint8_t)'F';
    pdu->protoLength = &buffer[1];
    protoPackedSize = KineticProto_get_packed_size(&message->proto);
    KineticPDU_PackInt64(pdu->protoLength, protoPackedSize); // Pack protobuf length field
    pdu->valueLength = &buffer[1 + sizeof(int64_t)];
    KineticPDU_PackInt64(pdu->valueLength, valueLength); // Pack value length field

    // Populate with the encoded kinetic protocol buffer
    pdu->proto = &buffer[PDU_HEADER_LEN];
    KineticProto_pack(&message->proto, pdu->proto);

    // Populate with the value data, if specified
    if (value == NULL)
    {
        pdu->value = NULL;
        pdu->valueLength = 0;
    }
    else
    {
        pdu->value = &buffer[PDU_HEADER_LEN + protoPackedSize];
        memcpy(pdu->value, value, valueLength);
    }

    // Associate PDU with message
    pdu->message = message;
}

void KineticPDU_PackInt64(uint8_t* const buffer, int64_t value)
{
    int i;
    for (i = sizeof(int64_t) - 1; i >= 0; i--)
    {
        buffer[i] = value & 0xFF;
        value = value >> 8;
    }
}

int64_t KineticPDU_UnpackInt64(const uint8_t* const buffer)
{
    int i;
    int64_t value = 0;
    for (i = 0; i < sizeof(int64_t); i++)
    {
        value <<= 8;
        value += buffer[i];
    }
    return value;
}
