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

KineticStatus KineticPDU_ReceiveMain(KineticPDU* const response)
{
    assert(response != NULL);
    assert(response->connection != NULL);
    const int fd = response->connection->socket;
    assert(fd >= 0);
    LOGF1("\nReceiving PDU via fd=%d", fd);

    KineticStatus status;
    KineticMessage* msg = &response->protoData.message;

    // Receive the PDU header
    ByteBuffer rawHeader =
        ByteBuffer_Create(&response->headerNBO, sizeof(KineticPDUHeader), 0);
    status = KineticSocket_Read(fd, &rawHeader, rawHeader.array.len);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to receive PDU header!");
        return status;
    }
    else {
        LOG3("PDU header received successfully");
        KineticPDUHeader* headerNBO = &response->headerNBO;
        response->header = (KineticPDUHeader) {
            .versionPrefix = headerNBO->versionPrefix,
            .protobufLength = KineticNBO_ToHostU32(headerNBO->protobufLength),
            .valueLength = KineticNBO_ToHostU32(headerNBO->valueLength),
        };
        KineticLogger_LogHeader(1, &response->header);
    }

    // Receive the protobuf message
    status = KineticSocket_ReadProtobuf(fd, response);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to receive PDU protobuf message!");
        return status;
    }
    else {
        LOG3("Received PDU protobuf");
    }

    // Validate the HMAC for the recevied protobuf message
    if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH) {
        KineticLogger_LogProtobuf(2, response->proto);
        if(!KineticHMAC_Validate(
          response->proto, response->connection->session.config.hmacKey)) {
            LOG0("Received PDU protobuf message has invalid HMAC!");
            msg->has_command = true;
            msg->command.status = &msg->status;
            msg->status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DATA_ERROR;
            return KINETIC_STATUS_DATA_ERROR;
        }
        else {
            LOG3("Received protobuf HMAC validation succeeded");
        }
    }
    else if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH) {
        KineticLogger_LogProtobuf(2, response->proto);
        LOG0("PIN-based authentication not yet supported!");
        return KINETIC_STATUS_DATA_ERROR;
    }
    else if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS) {
        KineticLogger_LogProtobuf(3, response->proto);
    }

    // Extract embedded command, if supplied
    KineticProto_Message* pMsg = response->proto;
    if (pMsg->has_commandBytes &&
      pMsg->commandBytes.data != NULL &&
      pMsg->commandBytes.len > 0) {
        response->command = KineticProto_command__unpack(
            NULL,
            pMsg->commandBytes.len,
            pMsg->commandBytes.data);
    }

    status = KineticPDU_GetStatus(response);
    if (status == KINETIC_STATUS_SUCCESS) {
        LOG2("PDU received successfully!");
    }
    return status;
}

KineticStatus KineticPDU_ReceiveValue(int socket_desc, ByteBuffer* value, size_t value_length)
{
    assert(socket_desc >= 0);
    assert(value != NULL);
    assert(value->array.data != NULL);

    // Receive value payload
    LOGF1("Receiving value payload (%lld bytes)...", value_length);
    ByteBuffer_Reset(value);
    KineticStatus status = KineticSocket_Read(socket_desc, value, value_length);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to receive PDU value payload!");
        return status;
    }
    LOG1("Received value payload successfully");
    KineticLogger_LogByteBuffer(3, "Value Buffer", *value);
    return status;
}

size_t KineticPDU_GetValueLength(KineticPDU* const pdu)
{
    assert(pdu != NULL);
    return (size_t)pdu->header.valueLength;
}

KineticStatus KineticPDU_GetStatus(KineticPDU* pdu)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (pdu != NULL &&
        pdu->command != NULL &&
        pdu->command->status != NULL &&
        pdu->command->status->has_code != false)
    {
        status = KineticProtoStatusCode_to_KineticStatus(
            pdu->command->status->code);
    }

    return status;
}

KineticProto_Command_KeyValue* KineticPDU_GetKeyValue(KineticPDU* pdu)
{
    KineticProto_Command_KeyValue* keyValue = NULL;
    if (pdu != NULL &&
        pdu->command != NULL &&
        pdu->command != NULL &&
        pdu->command->body != NULL)
    {
        keyValue = pdu->command->body->keyValue;
    }
    return keyValue;
}

KineticProto_Command_Range* KineticPDU_GetKeyRange(KineticPDU* pdu)
{
    KineticProto_Command_Range* range = NULL;
    if (pdu != NULL &&
        pdu->proto != NULL &&
        pdu->command != NULL &&
        pdu->command->body != NULL)
    {
        range = pdu->command->body->range;
    }
    return range;
}
