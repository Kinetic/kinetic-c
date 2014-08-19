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
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"

static void KineticPDU_PackInt32(uint8_t* const buffer, int32_t value);
static int32_t KineticPDU_UnpackInt32(const uint8_t* const buffer);

void KineticPDU_Init(KineticPDU* const pdu,
    KineticConnection* const connection,
    KineticMessage* const message,
    uint8_t* const value,
    int32_t valueLength)
{
    assert(pdu != NULL);
    assert(connection != NULL);

    // Create properly initialized PDU and populate passed instance
    const KineticPDU tmpPDU = {
        .connection = connection,
        .message = message,
        .proto = NULL,
        .protobufLength = 0,
        .value = value,
        .valueLength = valueLength,
        .header.versionPrefix = (uint8_t)'F' // Set header version prefix appropriately
    };
    *pdu = tmpPDU; // Copy initial value into target PDU

    // Configure the protobuf header with exchange info
    if (message != NULL)
    {
        KineticConnection_ConfigureHeader(pdu->connection, &message->header);
    }
}

KineticProto_Status_StatusCode KineticPDU_Status(KineticPDU* const pdu)
{
    assert(pdu != NULL);

    KineticProto_Status_StatusCode status = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;

    if (pdu->proto)
    {
        if (pdu->proto->command && pdu->proto->command->status)
        {
            status = pdu->proto->command->status->code;
        }
    }
    else if (&pdu->message->command && pdu->message->command.status)
    {
        status = pdu->message->command.status->code;
    }

    return status;
}

bool KineticPDU_Send(KineticPDU* const request)
{
    assert(request != NULL);
    assert(request->connection != NULL);
    assert(request->message != NULL);

    int fd = request->connection->socketDescriptor;

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(&request->hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&request->hmac, &request->message->proto, request->connection->key);

    // Repack PDU header length fields
    request->protobufLength = KineticProto_get_packed_size(&request->message->proto);
    LOGF("Packing PDU with protobuf of %d bytes", request->protobufLength);
    KineticPDU_PackInt32((uint8_t*)&request->header.protobufLength, request->protobufLength);
    LOGF("Packing PDU with value payload of %d bytes", request->valueLength);
    KineticPDU_PackInt32((uint8_t*)&request->header.valueLength, request->valueLength);

    // Send the PDU header
    if (!KineticSocket_Write(fd, &request->header, sizeof(KineticPDUHeader)))
    {
        LOG("Failed to send PDU header!");
        return false;
    }

    // Send the protobuf message
    LOG("Sending PDU Protobuf:");
    KineticLogger_LogProtobuf(&request->message->proto);
    if (!KineticSocket_WriteProtobuf(fd, &request->message->proto))
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
    const int fd = response->connection->socketDescriptor;
    LOGF("Attempting to receive PDU via fd=%d", fd);
    assert(fd >= 0);

    // Receive the PDU header
    if (!KineticSocket_Read(fd, response->rawHeader, sizeof(KineticPDUHeader)))
    {
        LOG("Failed to receive PDU header!");
        return false;
    }
    else
    {
        LOG("PDU header received successfully");
        response->header.versionPrefix = response->rawHeader[0];
        response->header.protobufLength = KineticPDU_UnpackInt32(&response->rawHeader[1]);
        response->header.valueLength = KineticPDU_UnpackInt32(&response->rawHeader[1 + sizeof(uint32_t)]);
        KineticLogger_LogHeader(&response->header);
    }

    // Receive the protobuf message
    if (!KineticSocket_ReadProtobuf(fd,
            &response->proto,
            (KineticProto*)response->protobufScratch,
            response->header.protobufLength))
    {
        LOG("Failed to receive PDU protobuf message!");
        return false;
    }
    else
    {
        LOG("Received PDU protobuf");
        KineticLogger_LogProtobuf(response->proto);
    }

    // Validate the HMAC for the recevied protobuf message
    if (!KineticHMAC_Validate(response->proto, response->connection->key))
    {
        LOG("Received PDU protobuf message has invalid HMAC!");
        return false;
    }
    else
    {
        LOG("Received protobuf HMAC validation succeeded");
    }

    // Receive the value payload, if specified
    if (response->header.valueLength > 0)
    {
        LOG("Attempting to receive value payload...");
        if (!KineticSocket_Read(fd, response->value, response->valueLength))
        {
            LOG("Failed to receive PDU value payload!");
            return false;
        }
        else
        {
            LOG("Received value payload successfully");
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
