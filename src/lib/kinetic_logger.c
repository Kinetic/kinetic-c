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
// #include "zlog/zlog.h"
#include <stdio.h>
#include <string.h>

static char LogFile[256] = "";
bool LogToConsole = true;
FILE* FileDesc = NULL;

void KineticLogger_Init(const char* logFile)
{
    LogToConsole = true;
    FileDesc = NULL;
    if (logFile != NULL) {
        strcpy(LogFile, logFile);
        FileDesc = fopen(LogFile, "w");
        if (FileDesc == NULL) {
            fprintf(stderr,
                    "Failed to initialize logger with file: "
                    "fopen('%s') => FileDesc=%zd\n",
                    logFile, (size_t)FileDesc);
        }
        else {
            fprintf(stderr,
                    "Logging output to %s\n",
                    logFile);
            LogToConsole = false;
        }
    }
}

void KineticLogger_Close(void)
{
    // Don't close std/already-opened streams
    if (!LogToConsole && strlen(LogFile) > 0 &&
        FileDesc != stdout && FileDesc != stderr && FileDesc != stdin) {
        fclose(FileDesc);
    }
}

void KineticLogger_Log(const char* message)
{
    if (message == NULL) {
        return;
    }

    FileDesc = LogToConsole ? stderr : fopen(LogFile, "a");
    if (FileDesc != NULL) {
        fprintf(FileDesc, "%s\n", message);
        fflush(FileDesc);
    }
}

int KineticLogger_LogPrintf(const char* format, ...)
{
    int result = -1;

    if (format != NULL) {
        va_list arg_ptr;
        char buffer[1024];
        va_start(arg_ptr, format);
        result = vsprintf(buffer, format, arg_ptr);
        KineticLogger_Log(buffer);
        va_end(arg_ptr);
    }

    return (result);
}


void KineticLogger_LogHeader(const KineticPDUHeader* header)
{
    LOG("PDU Header:");
    LOGF("  versionPrefix: %c", header->versionPrefix);
    LOGF("  protobufLength: %d", header->protobufLength);
    LOGF("  valueLength: %d", header->valueLength);
}

#define LOG_PROTO_INIT() \
    char _indent[32] = "  "; \
    const char* _str_true = "true"; \
    const char* _str_false = "false";

#define LOG_PROTO_LEVEL_START(name) \
    LOGF("%s" name " {", _indent); \
    strcat(_indent, "  ");

#define LOG_PROTO_LEVEL_END() \
    _indent[strlen(_indent) - 2] = '\0'; \
    LOGF("%s}", _indent);

int KineticLogger_u8toa(char* p_buf, uint8_t val)
{
    // LOGF("Converting byte=%02u", val);
    const int width = 2;
    int i = width;
    const uint8_t base = 16;
    char c = 0;

    p_buf += width - 1;
    do {
        c = val % base;
        val /= base;
        if (c >= 10) c += 'A' - '0' - 10;
        c += '0';
        // LOGF("CH: %c @ %d to 0x%llX", c, i, (long long)(p_buf));
        *p_buf-- = c;
    }
    while (--i);
    return width;
}

int KineticLogger_ByteArraySliceToCString(char* p_buf,
        const ByteArray bytes, const int start, const int count)
{
    // LOGF("Converting ByteArray (count=%u)", count);
    int len = 0;
    for (int i = 0; i < count; i++) {
        // LOGF("BYTE to 0x%llX (prepending '\\')", (long long)(&p_buf[len]));
        p_buf[len++] = '\\';
        // LOGF("BYTE digits to 0x%llX", (long long)(&p_buf[len]));
        len += KineticLogger_u8toa(&p_buf[len], bytes.data[start + i]);
        // LOGF("BYTE next @ 0x%llX", (long long)(&p_buf[len]));
    }
    p_buf[len] = '\0';
    // LOGF("BYTE string terminated @ 0x%llX", (long long)(&p_buf[len]));
    return len;
}

#define BYTES_TO_CSTRING(_buf_start, _array, _array_start, _count) { \
    char* p_buf = (_buf_start); \
    KineticLogger_ByteArraySliceToCString(p_buf, (_array), 0, _array.len); \
}

void KineticLogger_LogProtobuf(const KineticProto* proto)
{
    LOG_PROTO_INIT();
    char tmpBuf[1024];

    if (proto == NULL) {
        return;
    }

    LOG("Kinetic Protobuf:");
    if (proto->command) {
        LOG_PROTO_LEVEL_START("command");

        if (proto->command->header) {
            LOG_PROTO_LEVEL_START("header");
            {
                if (proto->command->header->has_clusterVersion) {
                    LOGF("%sclusterVersion: %lld", _indent,
                         proto->command->header->clusterVersion);
                }
                if (proto->command->header->has_identity) {
                    LOGF("%sidentity: %lld", _indent,
                         proto->command->header->identity);
                }
                if (proto->command->header->has_connectionID) {
                    LOGF("%sconnectionID: %lld", _indent,
                         proto->command->header->connectionID);
                }
                if (proto->command->header->has_sequence) {
                    LOGF("%ssequence: %lld", _indent,
                         proto->command->header->sequence);
                }
                if (proto->command->header->has_ackSequence) {
                    LOGF("%sackSequence: %lld", _indent,
                         proto->command->header->ackSequence);
                }
                if (proto->command->header->has_messageType) {
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                         &KineticProto_message_type__descriptor,
                                                         proto->command->header->messageType);
                    LOGF("%smessageType: %s", _indent, eVal->name);
                }
                if (proto->command->header->has_timeout) {
                    LOGF("%stimeout: %lld", _indent,
                         proto->command->header->ackSequence);
                }
                if (proto->command->header->has_earlyExit) {
                    LOGF("%searlyExit: %s", _indent,
                         proto->command->header->earlyExit ? _str_true : _str_false);
                }
                if (proto->command->header->has_backgroundScan) {
                    LOGF("%sbackgroundScan: %s", _indent,
                         proto->command->header->backgroundScan ? _str_true : _str_false);
                }
            }
            LOG_PROTO_LEVEL_END();
        }

        if (proto->command->body) {
            LOG_PROTO_LEVEL_START("body");
            {
                if (proto->command->body->keyValue) {
                    LOG_PROTO_LEVEL_START("keyValue");
                    {
                        if (proto->command->body->keyValue->has_key) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             proto->command->body->keyValue->key,
                                             0, proto->command->body->keyValue->key.len);
                            LOGF("%skey: '%s'", _indent, tmpBuf);
                        }
                        if (proto->command->body->keyValue->has_newVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             proto->command->body->keyValue->newVersion,
                                             0, proto->command->body->keyValue->newVersion.len);
                            LOGF("%snewVersion: '%s'", _indent, tmpBuf);
                        }
                        if (proto->command->body->keyValue->has_dbVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             proto->command->body->keyValue->dbVersion,
                                             0, proto->command->body->keyValue->dbVersion.len);
                            LOGF("%sdbVersion: '%s'", _indent, tmpBuf);
                        }
                        if (proto->command->body->keyValue->has_tag) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             proto->command->body->keyValue->tag,
                                             0, proto->command->body->keyValue->tag.len);
                            LOGF("%stag: '%s'", _indent, tmpBuf);
                        }
                        if (proto->command->body->keyValue->has_force) {
                            LOGF("%sforce: %s", _indent,
                                 proto->command->body->keyValue->force ? _str_true : _str_false);
                        }
                        if (proto->command->body->keyValue->has_algorithm) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_algorithm__descriptor,
                                                                 proto->command->body->keyValue->algorithm);
                            LOGF("%salgorithm: %s", _indent, eVal->name);
                        }
                        if (proto->command->body->keyValue->has_metadataOnly) {
                            LOGF("%smetadataOnly: %s", _indent,
                                 proto->command->body->keyValue->metadataOnly ? _str_true : _str_false);
                        }
                        if (proto->command->body->keyValue->has_synchronization) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_synchronization__descriptor,
                                                                 proto->command->body->keyValue->synchronization);
                            LOGF("%ssynchronization: %s", _indent, eVal->name);
                        }
                    }
                    LOG_PROTO_LEVEL_END();
                }
            }

            LOG_PROTO_LEVEL_END();
        }

        if (proto->command->status) {
            LOG_PROTO_LEVEL_START("status");
            {
                if (proto->command->status->has_code) {
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                         &KineticProto_status_status_code__descriptor,
                                                         proto->command->status->code);
                    LOGF("%scode: %s", _indent, eVal->name);
                }

                if (proto->command->status->statusMessage) {
                    LOGF("%sstatusMessage: '%s'", _indent, proto->command->status->statusMessage);
                }
                if (proto->command->status->has_detailedMessage) {
                    BYTES_TO_CSTRING(tmpBuf,
                                     proto->command->status->detailedMessage,
                                     0, proto->command->status->detailedMessage.len);
                    LOGF("%detailedMessage: '%s'", _indent, tmpBuf);
                }
            }
            LOG_PROTO_LEVEL_END();
        }
        LOG_PROTO_LEVEL_END();
    }

    if (proto->has_hmac) {
        BYTES_TO_CSTRING(tmpBuf,
                         proto->hmac,
                         0, proto->hmac->key.len);
        LOGF("%shmac: '%s'", _indent, tmpBuf);
    }
}

void KineticLogger_LogStatus(KineticProto_Status* status)
{
    ProtobufCMessage* protoMessage = (ProtobufCMessage*)status;
    KineticProto_Status_StatusCode code = status->code;

    if (code == KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS) {
        printf("Operation completed successfully\n");
    }
    else if (code == KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE) {
        printf("Operation was aborted!\n");
    }
    else {
        // Output status code short name
        const ProtobufCMessageDescriptor* protoMessageDescriptor = protoMessage->descriptor;
        const ProtobufCFieldDescriptor* statusCodeDescriptor =
            protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "code");
        const ProtobufCEnumDescriptor* statusCodeEnumDescriptor =
            (ProtobufCEnumDescriptor*)statusCodeDescriptor->descriptor;
        const ProtobufCEnumValue* eStatusCodeVal =
            protobuf_c_enum_descriptor_get_value(statusCodeEnumDescriptor, code);
        KineticLogger_LogPrintf("Operation completed but failed w/error: %s=%d(%s)\n",
                                statusCodeDescriptor->name, code, eStatusCodeVal->name);

        // Output status message, if supplied
        if (status->statusMessage) {
            const ProtobufCFieldDescriptor* statusMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "statusMessage");
            const ProtobufCMessageDescriptor* statusMsgDescriptor =
                (ProtobufCMessageDescriptor*)statusMsgFieldDescriptor->descriptor;

            KineticLogger_LogPrintf("  %s: '%s'", statusMsgDescriptor->name, status->statusMessage);
        }

        // Output detailed message, if supplied
        if (status->has_detailedMessage) {
            char tmp[8], msg[256];
            const ProtobufCFieldDescriptor* statusDetailedMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(
                    protoMessageDescriptor, "detailedMessage");
            const ProtobufCMessageDescriptor* statusDetailedMsgDescriptor =
                (ProtobufCMessageDescriptor*)
                statusDetailedMsgFieldDescriptor->descriptor;

            sprintf(msg, "  %s: ", statusDetailedMsgDescriptor->name);
            for (size_t i = 0; i < status->detailedMessage.len; i++) {
                sprintf(tmp, "%02hhX", status->detailedMessage.data[i]);
                strcat(msg, tmp);
            }
            KineticLogger_LogPrintf("  %s", msg);
        }
    }
}

void KineticLogger_LogByteArray(const char* title, ByteArray bytes)
{
    LOGF("%s: (%zd bytes)", title, bytes.len);
    const int byteChars = 4;
    const int bytesPerLine = 16;
    const int lineLen = 4 + (bytesPerLine * byteChars);
    char hex[lineLen + 1];
    char ascii[lineLen + 1];
    for (size_t i = 0; i < bytes.len;) {
        hex[0] = '\0';
        ascii[0] = '\0';
        for (int j = 0;
             j < bytesPerLine && i < bytes.len;
             j++, i++) {
            char byHex[8];
            sprintf(byHex, "%02hhX", bytes.data[i]);
            strcat(hex, byHex);
            char byAscii[8];
            int ch = (int)bytes.data[i];
            if (ch >= 32 && ch <= 126) {
                sprintf(byAscii, "%c", bytes.data[i]);
            }
            else {
                byAscii[0] = '.';
                byAscii[1] = '\0';
            }
            strcat(ascii, byAscii);
        }
        LOGF("  %s : %s", hex, ascii);
    }
}
