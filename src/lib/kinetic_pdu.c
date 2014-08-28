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
#include "kinetic_nbo.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"

void KineticPDU_Init(KineticPDU* const pdu,
    KineticConnection* const connection,
    KineticMessage* const message)
{
    KINETIC_PDU_INIT(pdu, connection, message);
}

void KineticPDU_AttachValuePayload(KineticPDU* const pdu,
    ByteArray payload)
{
    assert(pdu != NULL);
    pdu->value = payload;
}

void KineticPDU_EnableValueBuffer(KineticPDU* const pdu)
{
    assert(pdu != NULL);
    pdu->value.data = pdu->valueBuffer;
    pdu->value.len = sizeof(pdu->valueBuffer);
}

size_t KineticPDU_EnableValueBufferWithLength(KineticPDU* const pdu,
    size_t length)
{
    assert(pdu != NULL);

    pdu->value = BYTE_ARRAY_NONE;

    if (length <= sizeof(pdu->valueBuffer))
    {
        pdu->value = (ByteArray){.data = pdu->valueBuffer, .len = length};
    }

    return pdu->value.len;
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
    request->header.versionPrefix = 'F';
    request->header.valueLength = request->value.len;
    request->protobufLength = KineticProto__get_packed_size(&request->message->proto);
    request->header.protobufLength = request->protobufLength;
    KineticLogger_LogHeader(&request->header);
    request->header.protobufLength = KineticNBO_FromHostU32(request->protobufLength);
    request->header.valueLength = KineticNBO_FromHostU32(request->header.valueLength);

    // Pack and send the PDU header
    ByteArray header = {.data = (uint8_t*)&request->header, .len = sizeof(KineticPDUHeader)};
    if (!KineticSocket_Write(fd, header))
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
    if ((request->value.len > 0) && (request->value.data != NULL))
    {
        if (!KineticSocket_Write(fd, request->value))
        {
            LOG("Failed to send PDU value payload!");
            return false;
        }
    }

    return true;
}

bool KineticPDU_Receive(KineticPDU* const response)
{
    bool success = true;
    const int fd = response->connection->socketDescriptor;
    LOGF("Attempting to receive PDU via fd=%d", fd);
    assert(fd >= 0);
    assert(response != NULL);
    ByteArray rawHeader = {
        .data = (uint8_t*)&response->headerNBO,
        .len = sizeof(KineticPDUHeader) };

    // Receive the PDU header
    if (!KineticSocket_Read(fd, rawHeader))
    {
        LOG("Failed to receive PDU header!");
        return false;
    }
    else
    {
        LOG("PDU header received successfully");
        response->header = (KineticPDUHeader)
        {
            .versionPrefix = response->headerNBO.versionPrefix,
            .protobufLength = KineticNBO_ToHostU32(response->headerNBO.protobufLength),
            .valueLength = KineticNBO_ToHostU32(response->headerNBO.valueLength)
        };
        response->value.len = response->header.valueLength;
        KineticLogger_LogHeader(&response->header);
    }

    // Receive the protobuf message
    ByteArray protobuf = {
        .data = response->protobufScratch,
        .len = response->header.protobufLength
    };

    if (!KineticSocket_ReadProtobuf(fd,
            &response->proto,
            protobuf))
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
        // FIXME!! success = false;
        // .. but allow HMAC validation to report the error
        response->proto->command->status->code =
            KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR;
    }
    else
    {
        LOG("Received protobuf HMAC validation succeeded");
    }

    // Receive the value payload, if specified
    if (response->header.valueLength > 0)
    {
        assert(response->value.data != NULL);

        LOGF("Attempting to receive value payload (%lld bytes)...",
            (long long)response->header.valueLength);

        if (!KineticSocket_Read(fd, response->value))
        {
            LOG("Failed to receive PDU value payload!");
            success = false;
        }
        else
        {
            LOG("Received value payload successfully");
            KineticLogger_LogByteArray("Value Payload", response->value);
        }
    }

    return success;
}
