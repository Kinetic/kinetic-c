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

#include "kinetic_pdu.h"
#include "kinetic_nbo.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"

void KineticPDU_Init(KineticPDU* const pdu,
                     KineticConnection* const connection)
{
    KINETIC_PDU_INIT(pdu, connection);
}

void KineticPDU_AttachValuePayload(KineticPDU* const pdu,
                                   ByteArray payload)
{
    assert(pdu != NULL);
    pdu->value = payload;
}

bool KineticPDU_Send(KineticPDU* request)
{
    assert(request != NULL);
    assert(request->connection != NULL);
    LOGF("Attempting to send PDU via fd=%d", request->connection->socket);

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(&request->hmac,
                     KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&request->hmac,
                         &request->protoData.message.proto, request->connection->session.hmacKey);

    // Configure PDU header length fields
    request->header.versionPrefix = 'F';
    request->header.protobufLength =
        KineticProto__get_packed_size(&request->protoData.message.proto);
    request->header.valueLength =
        (request->metadata == NULL) ? 0 : request->metadata->value.len;
    KineticLogger_LogHeader(&request->header);

    // Create NBO copy of header for sending
    request->headerNBO.versionPrefix = 'F';
    request->headerNBO.protobufLength =
        KineticNBO_FromHostU32(request->header.protobufLength);
    request->headerNBO.valueLength =
        KineticNBO_FromHostU32(request->header.valueLength);

    // Pack and send the PDU header
    ByteArray headerNBO = {
        .data = (uint8_t*)&request->headerNBO,
        .len = sizeof(KineticPDUHeader)
    };
    if (!KineticSocket_Write(request->connection->socket, headerNBO)) {
        LOG("Failed to send PDU header!");
        return false;
    }

    // Send the protobuf message
    LOG("Sending PDU Protobuf:");
    KineticLogger_LogProtobuf(&request->protoData.message.proto);
    if (!KineticSocket_WriteProtobuf(request->connection->socket,
                                     request)) {
        LOG("Failed to send PDU protobuf message!");
        return false;
    }

    // Send the value/payload, if specified
    if ((request->value.len > 0) && (request->value.data != NULL)) {
        if (!KineticSocket_Write(request->connection->socket,
                                 request->value)) {
            LOG("Failed to send PDU value payload!");
            return false;
        }
    }

    return true;
}

bool KineticPDU_Receive(KineticPDU* const response)
{
    assert(response != NULL);
    assert(response->proto != NULL);
    const int fd = response->connection->socket;
    assert(fd >= 0);
    LOGF("Attempting to receive PDU via fd=%d", fd);
    ByteArray rawHeader = {
        .data = (uint8_t*)&response->headerNBO,
        .len = sizeof(KineticPDUHeader)
    };

    // Receive the PDU header
    if (!KineticSocket_Read(fd, rawHeader)) {
        LOG("Failed to receive PDU header!");
        return false;
    }
    else {
        LOG("PDU header received successfully");
        response->header = (KineticPDUHeader) {
            .versionPrefix = response->headerNBO.versionPrefix,
             .protobufLength =
                 KineticNBO_ToHostU32(response->headerNBO.protobufLength),
                 .valueLength =
                     KineticNBO_ToHostU32(response->headerNBO.valueLength)
        };
        response->value.len = response->header.valueLength;
        KineticLogger_LogHeader(&response->header);
    }

    // Receive the protobuf message
    if (!KineticSocket_ReadProtobuf(fd, response)) {
        LOG("Failed to receive PDU protobuf message!");
        return false;
    }
    else {
        LOG("Received PDU protobuf");
        KineticLogger_LogProtobuf(response->proto);
    }

    // Validate the HMAC for the recevied protobuf message
    if (!KineticHMAC_Validate(response->proto,
                              response->connection->session.hmacKey)) {
        LOG("Received PDU protobuf message has invalid HMAC!");
        response->protoData.message.proto.command =
            &response->protoData.message.command;
        response->protoData.message.command.status =
            &response->protoData.message.status;
        response->protoData.message.status.code =
            KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR;

        return false;
    }
    else {
        LOG("Received protobuf HMAC validation succeeded");
    }

    // Receive the value payload, if specified
    response->value.len = response->header.valueLength;
    if (response->header.valueLength > 0) {
        assert(response->value.data != NULL);

        LOGF("Receiving value payload (%lld bytes)...",
             (long long)response->header.valueLength);

        if (!KineticSocket_Read(fd, response->value)) {
            LOG("Failed to receive PDU value payload!");
            return false;
        }
        else {
            LOG("Received value payload successfully");
            response->value.len = response->header.valueLength;
            // KineticLogger_LogByteArray("Value Payload", response->value);
        }
    }

    // Update connectionID to match value returned from device, if provided
    if (response->proto->command != NULL &&
            response->proto->command->header != NULL &&
            response->proto->command->header->has_connectionID) {
        response->connection->connectionID =
            response->proto->command->header->connectionID;
    }

    return true;
}
