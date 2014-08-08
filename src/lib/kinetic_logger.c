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

#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_proto.h"
#include <stdio.h>
#include <string.h>

static char LogFile[256] = "";
bool LogToConsole = true;

void KineticLogger_Init(const char* logFile)
{
    LogToConsole = true;
    if (logFile != NULL)
    {
        FILE* fd;
        strcpy(LogFile, logFile);
        fd = fopen(LogFile, "w");
        if (fd != stdout && fd != stderr && fd != stdin)
        {
            fclose(fd);
            LogToConsole = false;
        }
        else
        {
            fprintf(stderr, "Failed to initialize logger with file: fopen('%s') => fd=%zd", logFile, (size_t)fd);
            fflush(fd);
        }
    }
}

void KineticLogger_Log(const char* message)
{
    FILE* fd = NULL;
    if (message == NULL)
    {
        return;
    }

    fd = LogToConsole ? stderr : fopen(LogFile, "a");
    if (fd >= 0)
    {
        fprintf(fd, "%s\n", message);
        fflush(fd);

        // Don't close std/already-opened streams
        if (LogFile != NULL && fd != stdout && fd != stderr && fd != stdin)
        {
            fclose(fd);
        }
    }
}

int KineticLogger_LogPrintf(const char* format, ...)
{
    int result = -1;

    if (format != NULL)
    {
        va_list arg_ptr;
        char buffer[1024];
        va_start(arg_ptr, format);
        result = vsprintf(buffer, format, arg_ptr);
        KineticLogger_Log(buffer);
        va_end(arg_ptr);
    }

   return(result);
}


void KineticLogger_LogHeader(const KineticPDUHeader* header)
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

void KineticLogger_LogProtobuf(const KineticProto* proto)
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
