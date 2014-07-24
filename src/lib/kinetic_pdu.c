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
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
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

bool KineticPDU_Send(KineticPDU* const request)
{
    int fd = request->exchange->connection->FileDescriptor;

    // Send the PDU header
    if (!KineticSocket_Write(fd, &request->header, sizeof(KineticPDUHeader)))
    {
        LOG("Failed to send PDU header!");
        return false;
    }

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(&request->hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&request->hmac, request->protobuf, request->exchange->key, request->exchange->keyLength);

    // Send the protobuf message
    if (!KineticSocket_WriteProtobuf(fd, request->protobuf))
    {
        LOG("Failed to send PDU protobuf message!");
        return false;
    }

    // Send the value/payload, if specified
    if ((request->valueLength > 0) && (request->value != NULL))
    {
        if (!KineticSocket_Write(fd, request->value, request->valueLength))
        {
            LOG("Failed to send PDU value payload!");
            return false;
        }
    }

    return true;
}

bool KineticPDU_Receive(KineticPDU* const response)
{
    const int fd = response->exchange->connection->FileDescriptor;

    // Send the PDU header
    if (!KineticSocket_Read(fd, &response->header, sizeof(KineticPDUHeader)))
    {
        LOG("Failed to receive PDU header!");
        return false;
    }

    // Send the protobuf message
    if (!KineticSocket_ReadProtobuf(fd, &response->protobuf->proto,
            (KineticProto*)response->protobufScratch,
            response->header.protobufLength))
    {
        LOG("Failed to receive PDU protobuf message!");
        return false;
    }

    // Validate the HMAC for the recevied protobuf message
    if (!KineticHMAC_Validate(&response->protobuf->proto,
            response->exchange->key, response->exchange->keyLength))
    {
        LOG("Received PDU protobuf message has invalid HMAC!");
        return false;
    }

    // Send the value payload, if specified
    if (response->header.valueLength)
    {
        if (!KineticSocket_Read(fd, response->value, response->valueLength))
        {
            LOG("Failed to receive PDU value payload!");
            return false;
        }
    }

    return true;
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
