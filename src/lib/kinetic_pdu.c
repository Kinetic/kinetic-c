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

#include "kinetic_types.h"
#include "kinetic_pdu.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"

static void KineticPDU_PackInt32(uint8_t* const buffer, int32_t value);
static int32_t KineticPDU_UnpackInt32(const uint8_t* const buffer);
static void KineticPDU_LogHeader(const KineticPDUHeader* header);
static void KineticPDU_LogProtobuf(const KineticProto* proto);

void KineticPDU_Init(
    KineticPDU* const pdu,
    KineticExchange* const exchange,
    KineticMessage* const message,
    uint8_t* const value,
    int32_t valueLength)
{
    size_t packedLength = 0;
    assert(pdu != NULL);
    assert(exchange != NULL);

    // Create properly initialized PDU and populate passed instance
    const KineticPDU tmpPDU = {
        .exchange = exchange,
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
        KineticExchange_ConfigureHeader(exchange, &message->header);
    }
}

bool KineticPDU_Send(KineticPDU* const request)
{
    int fd = request->exchange->connection->socketDescriptor;

    assert(request != NULL);
    assert(request->message != NULL);

    // Populate the HMAC for the protobuf
    KineticHMAC_Init(&request->hmac, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&request->hmac, &request->message->proto, request->exchange->key, request->exchange->keyLength);

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
    KineticPDU_LogProtobuf(&request->message->proto);
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
    const int fd = response->exchange->connection->socketDescriptor;
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
        KineticPDU_LogHeader(&response->header);
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
        KineticPDU_LogProtobuf(response->proto);
    }

    // Validate the HMAC for the recevied protobuf message
    if (!KineticHMAC_Validate(response->proto,
            response->exchange->key, response->exchange->keyLength))
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

void KineticPDU_LogHeader(const KineticPDUHeader* header)
{
    LOG("PDU Header:");
    LOGF("  versionPrefix: %c", header->versionPrefix);
    LOGF("  protobufLength: %u", header->protobufLength);
    LOGF("  valueLength: %u", header->valueLength);
}

#define LOG_PROTO_INIT() \
    unsigned int _i; \
    char _indent[32] = "  "; \
    char _buf[1024]; \
    const char* _str_true = "true"; \
    const char* _str_false = "false";

#define LOG_PROTO_LEVEL_START(name) \
    LOGF("%s" name " {", _indent); \
    strcat(_indent, "  ");

#define LOG_PROTO_LEVEL_END() \
    _indent[strlen(_indent) - 2] = '\0'; \
    LOGF("%s}", _indent);

void KineticPDU_LogProtobuf(const KineticProto* proto)
{
    LOG_PROTO_INIT();

    if (proto == NULL)
    {
        return;
    }

    LOG("Kinetic Protobuf:");

    // KineticProto_Command* command;
    if (proto->command)
    {
        LOG_PROTO_LEVEL_START("command");

        if (proto->command->header)
        {
            LOG_PROTO_LEVEL_START("header");
            {
                if (proto->command->header->has_clusterversion)
                {
                    LOGF("%sclusterVersion: %lld", _indent,
                        proto->command->header->clusterversion);
                }
                if (proto->command->header->has_identity)
                {
                    LOGF("%sidentity: %lld", _indent,
                        proto->command->header->identity);
                }
                if (proto->command->header->has_connectionid)
                {
                    LOGF("%sconnectionID: %lld", _indent,
                        proto->command->header->connectionid);
                }
                if (proto->command->header->has_sequence)
                {
                    LOGF("%ssequence: %lld", _indent,
                        proto->command->header->sequence);
                }
                if (proto->command->header->has_acksequence)
                {
                    LOGF("%sackSequence: %lld", _indent,
                        proto->command->header->acksequence);
                }
                if (proto->command->header->has_messagetype)
                {
                    // KineticProto_MessageType messagetype;
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                        &KineticProto_message_type_descriptor,
                        proto->command->header->messagetype);
                    LOGF("%smessagetype: %s", _indent, eVal->name);
                }
                if (proto->command->header->has_identity)
                {
                    LOGF("%sidentity: %lld", _indent,
                        proto->command->header->identity);
                }
                if (proto->command->header->has_timeout)
                {
                    LOGF("%stimeout: %s", _indent, 
                        proto->command->header->timeout ? _str_true : _str_false);
                }
                if (proto->command->header->has_earlyexit)
                {
                    LOGF("%searlyExit: %s", _indent, 
                        proto->command->header->earlyexit ? _str_true : _str_false);
                }
                if (proto->command->header->has_backgroundscan)
                {
                    LOGF("%sbackgroundScan: %s", _indent, 
                        proto->command->header->backgroundscan ? _str_true : _str_false);
                }      
            }
            LOG_PROTO_LEVEL_END();
        }

        if (proto->command->body)
        {
            LOG_PROTO_LEVEL_START("body");
            LOG_PROTO_LEVEL_END();
        }

        if (proto->command->status)
        {
            LOG_PROTO_LEVEL_START("body");
            {
                if (proto->command->status->has_code)
                {
                    // KineticProto_Status_StatusCode code;
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                        &KineticProto_status_status_code_descriptor,
                        proto->command->status->code);
                    LOGF("%scode: %s", _indent, eVal->name);
                }

                if (proto->command->status->statusmessage)
                {
                    LOGF("%sstatusMessage: '%s'",
                        _indent, proto->command->status->statusmessage);
                }
                if (proto->command->status->has_detailedmessage)
                {
                    char tmp[8], buf[64] = "detailedMessage: ";
                    for (_i = 0; _i < proto->command->status->detailedmessage.len; _i++)
                    {
                        sprintf(tmp, "%02hhX", proto->command->status->detailedmessage.data[_i]);
                        strcat(buf, tmp);
                    }
                    LOGF("%s%s", _indent, buf);
                }
            }
            LOG_PROTO_LEVEL_END();
        }
        LOG_PROTO_LEVEL_END();
    }
    
    if (proto->has_hmac)
    {
        char tmp[8], buf[64] = "hmac: ";
        for (_i = 0; _i < proto->hmac.len; _i++)
        {
            sprintf(tmp, "%02hhX", proto->hmac.data[_i]);
            strcat(buf, tmp);
        }
        LOGF("%s%s", _indent, buf);
    }
}
