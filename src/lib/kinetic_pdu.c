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
#include <stdlib.h>

// #define KINETIC_LOG_PDU_OPERATIONS

void KineticPDU_Init(KineticPDU* const pdu,
                     KineticConnection* const connection)
{
    KINETIC_PDU_INIT(pdu, connection);
}

void KineticPDU_AttachEntry(KineticPDU* const pdu, KineticEntry* const entry)
{
    assert(pdu != NULL);
    assert(entry != NULL);
    pdu->entry = *entry;
}

KineticStatus KineticPDU_Send(KineticPDU* request)
{
    assert(request != NULL);
    assert(request->connection != NULL);
    LOGF1("\nSending PDU via fd=%d", request->connection->socket);
    KineticStatus status = KINETIC_STATUS_INVALID;
    request->proto = &request->protoData.message.message;

    // Pack the command, if available
    if (request->protoData.message.has_command) {
        size_t expectedLen = 
            KineticProto_command__get_packed_size(&request->protoData.message.command);
        request->protoData.message.message.commandBytes.data = 
            (uint8_t*)malloc(expectedLen);
        assert(request->protoData.message.message.commandBytes.data != NULL);
        size_t packedLen = KineticProto_command__pack(
            &request->protoData.message.command,
            request->protoData.message.message.commandBytes.data);
        assert(packedLen == expectedLen);
        request->protoData.message.message.commandBytes.len = packedLen;
        request->protoData.message.message.has_commandBytes = true;
        KineticLogger_LogByteArray(2, "commandBytes", (ByteArray){
            .data = request->protoData.message.message.commandBytes.data,
            .len = request->protoData.message.message.commandBytes.len,
        });
    }

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(
        &request->hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(
        &request->hmac, request->proto, request->connection->session.hmacKey);

    // Configure PDU header length fields
    request->header.versionPrefix = 'F';
    request->header.protobufLength =
        KineticProto_Message__get_packed_size(request->proto);
    request->header.valueLength =
        (request->entry.value.array.data == NULL) ? 0 : request->entry.value.bytesUsed;
    KineticLogger_LogHeader(1, &request->header);

    // Create NBO copy of header for sending
    request->headerNBO.versionPrefix = 'F';
    request->headerNBO.protobufLength =
        KineticNBO_FromHostU32(request->header.protobufLength);
    request->headerNBO.valueLength =
        KineticNBO_FromHostU32(request->header.valueLength);

    // Pack and send the PDU header
    ByteBuffer hdr = ByteBuffer_Create(&request->headerNBO, sizeof(KineticPDUHeader), sizeof(KineticPDUHeader));
    status = KineticSocket_Write(request->connection->socket, &hdr);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to send PDU header!");
        return status;
    }

    // Send the protobuf message
    LOG1("Sending PDU Protobuf:");
    KineticLogger_LogProtobuf(2, request->proto);
    status = KineticSocket_WriteProtobuf(
                 request->connection->socket, request);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Failed to send PDU protobuf message!");
        return status;
    }

    // Send the value/payload, if specified
    ByteBuffer* value = &request->entry.value;
    if ((value->array.data != NULL) && (value->array.len > 0) && (value->bytesUsed > 0)) {
        LOGF1("Sending PDU Value Payload (%zu bytes)", value->bytesUsed);
        status = KineticSocket_Write(request->connection->socket, value);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG0("Failed to send PDU value payload!");
            return status;
        }
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticPDU_Receive(KineticPDU* const response)
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
        LOG2("PDU header received successfully");
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
        LOG2("Received PDU protobuf");
        KineticLogger_LogProtobuf(2, response->proto);
    }

    // Validate the HMAC for the recevied protobuf message
    if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH) {
        if(!KineticHMAC_Validate(
          response->proto, response->connection->session.hmacKey)) {
            LOG0("Received PDU protobuf message has invalid HMAC!");
            msg->has_command = true;
            msg->command.status = &msg->status;
            msg->status.code = KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_DATA_ERROR;
            return KINETIC_STATUS_DATA_ERROR;
        }
        else {
            LOG2("Received protobuf HMAC validation succeeded");
        }
    }
    else if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH) {
        LOG0("PIN-based authentication not yet supported!");
        return KINETIC_STATUS_DATA_ERROR;
    }
    else if (response->proto->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_UNSOLICITEDSTATUS) {
        LOG1("Unsolicited status message is not authenticated");
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

    // Receive the value payload, if specified
    if (response->header.valueLength > 0) {
        assert(response->entry.value.array.data != NULL);
        LOGF1("Receiving value payload (%lld bytes)...",
             (long long)response->header.valueLength);
        ByteBuffer_Reset(&response->entry.value);
        status = KineticSocket_Read(fd,
            &response->entry.value, response->header.valueLength);
        if (status != KINETIC_STATUS_SUCCESS) {
            LOG0("Failed to receive PDU value payload!");
            return status;
        }
        else {
            LOG1("Received value payload successfully");
            KineticLogger_LogByteBuffer(2, "ACTUAL VALUE RECEIVED", response->entry.value);
        }
        KineticLogger_LogByteBuffer(2, "Value Buffer", response->entry.value);
    }

    // Update connectionID to match value returned from device, if provided
    KineticProto_Command* cmd = &response->protoData.message.command;
    if ((msg->has_command) && (cmd->header != NULL) && (cmd->header->has_connectionID)) {
        response->connection->connectionID = cmd->header->connectionID;
    }

    return KineticPDU_GetStatus(response);
}

KineticStatus KineticPDU_GetStatus(KineticPDU* pdu)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (pdu != NULL &&
        pdu->command != NULL &&
        pdu->command->status != NULL &&
        pdu->command->status->has_code != false) {

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
        pdu->command->body != NULL) {

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
        pdu->command->body != NULL) {

        range = pdu->command->body->range;
    }
    return range;
}
