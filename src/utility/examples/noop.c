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

#include "noop.h"

int NoOp(const char* host, int port, int64_t clusterVersion, int64_t identity, const char* key)
{
    KineticExchange exchange;
    KineticOperation operation;
    KineticPDU request, response;
    KineticConnection connection;
    KineticMessage requestMsg;
    KineticProto_Status_StatusCode status;
    bool success;

    KineticApi_Init(NULL);

    success = KineticApi_Connect(&connection, host, port, true);
    assert(success);
    assert(connection.socketDescriptor >= 0);

    success = KineticApi_ConfigureExchange(&exchange, &connection, clusterVersion, identity, key, strlen(key));
    assert(success);

    operation = KineticApi_CreateOperation(&exchange, &request, &requestMsg, &response);

    status = KineticApi_NoOp(&operation);

    if (status == KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS)
    {
        printf("NoOp operation completed successfully. Kinetic Device is alive and well!\n");
        return 0;
    }
    else
    {
        const KineticProto_Status* protoStatus = response.proto->command->status;
        const ProtobufCMessage* protoMessage = (ProtobufCMessage*)protoStatus;
        const ProtobufCMessageDescriptor* protoMessageDescriptor = protoMessage->descriptor;
        const ProtobufCFieldDescriptor* protoMessageFields = protoMessageDescriptor->fields;

        // Output status code short name
        const ProtobufCFieldDescriptor* statusCodeDescriptor =
            protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "code");
        const ProtobufCEnumDescriptor* statusCodeEnumDescriptor =
            (ProtobufCEnumDescriptor*)statusCodeDescriptor->descriptor;
        const ProtobufCEnumValue* eStatusCodeVal =
            protobuf_c_enum_descriptor_get_value(statusCodeEnumDescriptor, status);
        printf("NoOp operation completed but failed w/error: %s=%d(%s)\n",
            statusCodeDescriptor->name, status, eStatusCodeVal->name);

        // Output status message, if supplied
        if (protoStatus->statusmessage)
        {
            const ProtobufCFieldDescriptor* statusMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "statusMessage");
            const ProtobufCMessageDescriptor* statusMsgDescriptor =
                (ProtobufCMessageDescriptor*)statusMsgFieldDescriptor->descriptor;

            printf("  %s: '%s'", statusMsgDescriptor->name, protoStatus->statusmessage);
        }

        // Output detailed message, if supplied
        if (protoStatus->has_detailedmessage)
        {
            char tmp[8], msg[256];
            const ProtobufCFieldDescriptor* statusDetailedMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "detailedMessage");
            const ProtobufCMessageDescriptor* statusDetailedMsgDescriptor =
                (ProtobufCMessageDescriptor*)statusDetailedMsgFieldDescriptor->descriptor;
            
            sprintf(msg, "  %s: ", statusDetailedMsgDescriptor->name);
            for (int i = 0; i < protoStatus->detailedmessage.len; i++)
            {
                sprintf(tmp, "%02hhX", protoStatus->detailedmessage.data[i]);
                strcat(msg, tmp);
            }
            printf("  %s", msg);
        }

        return status;
    }
}
