/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "kinetic_request.h"
#include <pthread.h>

#include "kinetic_logger.h"
#include "kinetic_session.h"
#include "kinetic_auth.h"
#include "kinetic_nbo.h"
#include "kinetic_controller.h"
#include "byte_array.h"
#include "bus.h"

#ifdef TEST
uint8_t *cmdBuf = NULL;
uint8_t *msg = NULL;
#endif

size_t KineticRequest_PackCommand(KineticRequest* request)
{
    size_t expectedLen = com__seagate__kinetic__proto__command__get_packed_size(&request->message.command);
    #ifndef TEST
    uint8_t *cmdBuf = (uint8_t*)malloc(expectedLen);
    #endif
    if (cmdBuf == NULL)
    {
        LOGF0("Failed to allocate command bytes: %zd", expectedLen);
        return KINETIC_REQUEST_PACK_FAILURE;
    }
    request->message.message.commandbytes.data = cmdBuf;

    size_t packedLen = com__seagate__kinetic__proto__command__pack(
        &request->message.command, cmdBuf);
    KINETIC_ASSERT(packedLen == expectedLen);
    request->message.message.commandbytes.len = packedLen;
    request->message.message.has_commandbytes = true;
    KineticLogger_LogByteArray(3, "commandbytes", (ByteArray){
        .data = request->message.message.commandbytes.data,
        .len = request->message.message.commandbytes.len,
    });

    return packedLen;
}

KineticStatus KineticRequest_PopulateAuthentication(KineticSessionConfig *config,
    KineticRequest *request, ByteArray *pin)
{
    if (pin != NULL) {
        return KineticAuth_PopulatePin(config, request, *pin);
    } else {
        return KineticAuth_PopulateHmac(config, request);
    }
}

KineticStatus KineticRequest_PackMessage(KineticOperation *operation,
    uint8_t **out_msg, size_t *msgSize)
{
    // Configure PDU header
    Com__Seagate__Kinetic__Proto__Message* proto = &operation->request->message.message;
    KineticPDUHeader header = {
        .versionPrefix = 'F',
        .protobufLength = com__seagate__kinetic__proto__message__get_packed_size(proto)
    };
    header.valueLength = operation->value.len;
    uint32_t nboProtoLength = KineticNBO_FromHostU32(header.protobufLength);
    uint32_t nboValueLength = KineticNBO_FromHostU32(header.valueLength);

    // Allocate and pack protobuf message
    size_t offset = 0;
    #ifndef TEST
    uint8_t *msg = malloc(PDU_HEADER_LEN + header.protobufLength + header.valueLength);
    #endif
    if (msg == NULL) {
        LOG0("Failed to allocate outgoing message!");
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    // Pack header
    KineticRequest* request = operation->request;
    msg[offset] = header.versionPrefix;
    offset += sizeof(header.versionPrefix);
    memcpy(&msg[offset], &nboProtoLength, sizeof(nboProtoLength));
    offset += sizeof(nboProtoLength);
    memcpy(&msg[offset], &nboValueLength, sizeof(nboValueLength));
    offset += sizeof(nboValueLength);
    size_t len = com__seagate__kinetic__proto__message__pack(&request->message.message, &msg[offset]);
    KINETIC_ASSERT(len == header.protobufLength);
    offset += header.protobufLength;

    #ifndef TEST
    // Log protobuf per configuration
    LOGF2("[PDU TX] pdu: %p, session: %p, bus: %p, "
        "fd: %6d, seq: %8lld, protoLen: %8u, valueLen: %8u, op: %p, msgType: %02x",
        (void*)operation->request,
        (void*)operation->session, (void*)operation->session->messageBus,
        operation->session->socket, (long long)request->message.header.sequence,
        header.protobufLength, header.valueLength,
        (void*)operation, request->message.header.messagetype);
    KineticLogger_LogHeader(3, &header);
    KineticLogger_LogProtobuf(3, proto);
    #endif

    // Pack value payload, if supplied
    if (header.valueLength > 0) {
        memcpy(&msg[offset], operation->value.data, operation->value.len);
        offset += operation->value.len;
    }
    KINETIC_ASSERT((PDU_HEADER_LEN + header.protobufLength + header.valueLength) == offset);

    *out_msg = msg;
    *msgSize = offset;
    return KINETIC_STATUS_SUCCESS;
}

bool KineticRequest_SendRequest(KineticOperation *operation,
    uint8_t *msg, size_t msgSize)
{
    KINETIC_ASSERT(msg);
    KINETIC_ASSERT(msgSize > 0);
    bus_user_msg bus_msg = {
        .fd       = operation->session->socket,
        .type     = BUS_SOCKET_PLAIN,
        .seq_id   = operation->request->message.header.sequence,
        .msg      = msg,
        .msg_size = msgSize,
        .cb       = KineticController_HandleResult,
        .udata    = operation,
        .timeout_sec = operation->timeoutSeconds,
    };
    return Bus_SendRequest(operation->session->messageBus, &bus_msg);
}

bool KineticRequest_LockSend(KineticSession* session)
{
    KINETIC_ASSERT(session);
    return 0 == pthread_mutex_lock(&session->sendMutex);
}

bool KineticRequest_UnlockSend(KineticSession* session)
{
    KINETIC_ASSERT(session);
    return 0 == pthread_mutex_unlock(&session->sendMutex);
}
