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

#include "kinetic_logger.h"
#include "zlog/zlog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define USE_GENERIC_LOGGER 1

int LogLevel = -1;

void KineticLogger_Init(const char* logFile)
{
    if (logFile == NULL) {
        LogLevel = -1;
        printf("\nLogging kinetic-c output is disabled!\n");
        return;
    }
    else {
        LogLevel = 0;
        
        if (strncmp(logFile, "stdout", 4) == 0 || strncmp(logFile, "STDOUT", 4) == 0) {
            printf("\nLogging kinetic-c output to console (stdout)\n");
            zlog_init_stdout();
        }
        else {
            zlog_init(logFile);
            printf("\nLogging kinetic-c output to %s\n", logFile);
        }

        // Create flushing thread to periodically flush the log
        // zlog_init_flush_thread();
    }
}

void KineticLogger_Close(void)
{
    if (LogLevel >= 0) {
        zlog_finish();
    }
}

void KineticLogger_Log(const char* message)
{
    if (message == NULL || LogLevel < 0) {
        return;
    }
    zlogf(message);
    zlogf("\n");
    zlog_flush_buffer();
}

void KineticLogger_LogPrintf(const char* format, ...)
{
    if (LogLevel < 0 || format == NULL) {
        return;
    }
    char buffer[128];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsnprintf(buffer, sizeof(buffer), format, arg_ptr);
    va_end(arg_ptr);
    strcat(buffer, "\n");
    zlogf(buffer);
    zlog_flush_buffer();
}

void KineticLogger_LogLocation(char* filename, int line, char const * format, ...)
{
    if (LogLevel >= 0) {
        va_list arg_ptr;
        va_start(arg_ptr, format);
        zlogf("[@%s:%d] %s\n", filename, line, format, arg_ptr);
        va_end(arg_ptr);
        zlog_flush_buffer();
    }
}

void KineticLogger_LogHeader(const KineticPDUHeader* header)
{
    if (LogLevel < 0) {
        return;
    }

    LOG("PDU Header:");
    LOGF("  versionPrefix: %c", header->versionPrefix);
    LOGF("  protobufLength: %d", header->protobufLength);
    LOGF("  valueLength: %d", header->valueLength);
}

const char* _str_true = "true";
const char* _str_false = "false";

#define LOG_PROTO_INIT() char _indent[32] = "  ";

#define LOG_PROTO_LEVEL_START(__name) \
    LOGF("%s%s {", (_indent), (__name)); \
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
        *p_buf-- = c;
    }
    while (--i);
    return width;
}

int KineticLogger_ByteArraySliceToCString(char* p_buf,
        const ByteArray bytes, const int start, const int count)
{
    int len = 0;
    for (int i = 0; i < count; i++) {
        p_buf[len++] = '\\';
        len += KineticLogger_u8toa(&p_buf[len], bytes.data[start + i]);
    }
    p_buf[len] = '\0';
    return len;
}

#define BYTES_TO_CSTRING(_buf_start, _array, _array_start, _count) { \
    ByteArray __array = {.data = _array.data, .len = (_array).len}; \
    KineticLogger_ByteArraySliceToCString((char*)(_buf_start), (__array), (_array_start), (_count)); \
}

// #define Proto_LogBinaryDataOptional(el, attr)
// if ((el)->has_##(attr)) {
//     KineticLogger_LogByteArray(#attr, (el)->(attr));
// }

#if USE_GENERIC_LOGGER
static void KineticLogger_LogProtobufMessage(const ProtobufCMessage *msg, char* _indent)
{
    assert(msg != NULL);
    assert(msg->descriptor != NULL);

    const ProtobufCMessageDescriptor* desc = msg->descriptor;
    const uint8_t* pMsg = (const uint8_t*)msg;
    // char tmpBuf[1024];

    LOG_PROTO_LEVEL_START(desc->short_name);
    // LOGF("%s* ProtobufMessage: msg=0x%0llX, _indent, range=0x%0llX, name=%s, fields=%u",
    //     _indent, msg, pMsg, desc->short_name, desc->n_fields);

    for (unsigned int i = 0; i < desc->n_fields; i++) {
        bool indexed = false;
        const ProtobufCFieldDescriptor* fieldDesc =
            (const ProtobufCFieldDescriptor*)((uint8_t*)&desc->fields[i]);
        // const uint8_t* pVal = pMsg + fieldDesc->offset;
        const uint8_t* quantifier = pMsg + fieldDesc->quantifier_offset;
        const uint32_t quantity = *((uint32_t*)quantifier);

        if (fieldDesc == NULL) {
            continue;
        }

        // LOGF("%s * ProtobufField[%s]: descriptor=0x%0llX",
        //     _indent, fieldDesc->name, fieldDesc);
        // LOGF("%s   * value: field=0x%0llX, offset=%u, value=?, type=%d",
        //     _indent, pVal, fieldDesc->offset, fieldDesc->type);
        // LOGF("%s   * quantifier: quantifier=0x%0llX, offset=%u, quantity=%u",
        //     _indent, quantifier, fieldDesc->quantifier_offset, quantity);

        switch (fieldDesc->type) {
        case PROTOBUF_C_TYPE_INT32:
        case PROTOBUF_C_TYPE_SINT32:
        case PROTOBUF_C_TYPE_SFIXED32:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                int32_t* value = (int32_t*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %ld", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_INT64:
        case PROTOBUF_C_TYPE_SINT64:
        case PROTOBUF_C_TYPE_SFIXED64:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                int64_t* value = (int64_t*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %lld", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_UINT32:
        case PROTOBUF_C_TYPE_FIXED32:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                uint32_t* value = (uint32_t*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %lu", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_UINT64:
        case PROTOBUF_C_TYPE_FIXED64:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                uint64_t* value = (uint64_t*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %llu", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_FLOAT:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                float* value = (float*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %f", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_DOUBLE:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                double* value = (double*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %f", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_BOOL:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                protobuf_c_boolean* value = (protobuf_c_boolean*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %s", _indent, fieldDesc->name, suffix,
                        value[i] ? _str_true : _str_false);
            }
            break;

        case PROTOBUF_C_TYPE_STRING:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                char** strings = (char**)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %s", _indent, fieldDesc->name, suffix, strings[i]);
            }
            break;

        case PROTOBUF_C_TYPE_BYTES:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                ProtobufCBinaryData* array = (ProtobufCBinaryData*)(msg + fieldDesc->offset + (i * sizeof(ProtobufCBinaryData)));
                LOGF("%s%s%s: addr=0x%0llX, data=0x%0llX, len=%zu",
                    _indent, fieldDesc->name, suffix, array, array->data, array->len);
            }
            break;

            // if (quantity > 0) {
            //     LOGF("%s* TYPE: PROTOBUF_C_TYPE_BYTES (incomplete!)", _indent);
            //     // ProtobufCBinaryData* pArray = *(ProtobufCBinaryData**)((unsigned long)(pVal));
            //     // LOGF("%s    * pArray=0x%0llX, points to: 0x%0llX", _indent, pArray, *pArray);
            //     // ProtobufCBinaryData* arrays = (ProtobufCBinaryData*)pArray;
            //     // // arrays++;
            //     // LOGF("%s    * BYTE_ARRAYS: raw=0x%0llX, arrays=0x%0llX, quantity=%u, &[0]=0x%0llX, &[1]=0x%0llX",
            //     //     _indent, pVal, arrays, quantity, &arrays[0], &arrays[1]);
            //     // for (unsigned int i = 0; i < quantity; i++) {
            //     //     ProtobufCBinaryData* myArray = (ProtobufCBinaryData*)&arrays[i];
            //     //     myArray += i;
            //     //     LOGF("%s     * data=0x%0llX, len=%u", _indent, myArray->data, myArray->len);
            //     //     if (myArray->data != NULL && myArray->len > 0) {
            //     //         char suffix[32] = {'\0'};
            //     //         if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
            //     //         // BYTES_TO_CSTRING(tmpBuf, &myArray, 0, myArray->len);
            //     //         // LOGF("%s%s%s: %s", _indent, fieldDesc->name, suffix, tmpBuf);
            //     //     }
            //     // }
            // }
            // break;

        case PROTOBUF_C_TYPE_ENUM:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                ProtobufCEnumValue* enumVal = (ProtobufCEnumValue*)(msg + fieldDesc->offset);
                LOGF("%s%s%s: %s", _indent, fieldDesc->name, suffix, enumVal[i].name);
            }
            break;

        case PROTOBUF_C_TYPE_MESSAGE:  // nested message
            LOG("Nested messages not yet supported!");
            assert(false); // nested messages not yet handled!
            break;

        default:
            LOG("Invalid message field type!");
            assert(false); // should never get here!
            break;
        };
    }

    LOG_PROTO_LEVEL_END();
}
#endif

void KineticLogger_LogProtobuf(const KineticProto_Message* msg)
{
    if (LogLevel < 0 || msg == NULL) {
        return;
    }

    LOG_PROTO_INIT();
// #if USE_GENERIC_LOGGER
// #else
    char tmpBuf[1024];
// #endif

    LOG("Kinetic Protobuf:");

    if (msg->has_authType) {
        const ProtobufCEnumValue* eVal = 
            protobuf_c_enum_descriptor_get_value(
                &KineticProto_Message_auth_type__descriptor,
                msg->authType);
        LOGF("%sauthType: %s", _indent, eVal->name);

        if (msg->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH && msg->hmacAuth != NULL) {
        #if USE_GENERIC_LOGGER
            KineticLogger_LogProtobufMessage((ProtobufCMessage*)msg->hmacAuth, _indent);
        #else
            LOG_PROTO_LEVEL_START("hmacAuth");
            if (msg->hmacAuth->has_identity) {
                LOGF("%sidentity: %lld", _indent, msg->hmacAuth->identity);
            }

            if (msg->hmacAuth->has_hmac) {
                BYTES_TO_CSTRING(tmpBuf, msg->hmacAuth->hmac, 0, msg->hmacAuth->hmac.len);
                LOGF("%shmac: '%s'", _indent, tmpBuf);
            }

            LOG_PROTO_LEVEL_END();
        #endif
        }
        else if (msg->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH && msg->pinAuth != NULL) {
        #if USE_GENERIC_LOGGER
            KineticLogger_LogProtobufMessage((ProtobufCMessage*)msg->hmacAuth, _indent);
        #else
            LOG_PROTO_LEVEL_START("pinAuth");

            if (msg->pinAuth->has_pin) {
                BYTES_TO_CSTRING(tmpBuf, msg->pinAuth->pin, 0, msg->pinAuth->pin.len);
                LOGF("%spin: '%s'", _indent, tmpBuf);
            }

            LOG_PROTO_LEVEL_END();
        #endif
        }
    }

    if (msg->has_commandBytes
      && msg->commandBytes.data != NULL
      && msg->commandBytes.len > 0) {
        LOG_PROTO_LEVEL_START("commandBytes");
        KineticProto_Command* cmd = KineticProto_command__unpack(NULL, msg->commandBytes.len, msg->commandBytes.data);

        if (cmd->header) {
        #if 0 //USE_GENERIC_LOGGER
            KineticLogger_LogProtobufMessage((ProtobufCMessage*)cmd->header, _indent);
        #else
            LOG_PROTO_LEVEL_START("header");
            {
                if (cmd->header->has_clusterVersion) {
                    LOGF("%sclusterVersion: %lld", _indent,
                         cmd->header->clusterVersion);
                }
                if (cmd->header->has_connectionID) {
                    LOGF("%sconnectionID: %lld", _indent,
                         cmd->header->connectionID);
                }
                if (cmd->header->has_sequence) {
                    LOGF("%ssequence: %lld", _indent,
                         cmd->header->sequence);
                }
                if (cmd->header->has_ackSequence) {
                    LOGF("%sackSequence: %lld", _indent,
                         cmd->header->ackSequence);
                }
                if (cmd->header->has_messageType) {
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                         &KineticProto_command_message_type__descriptor,
                                                         cmd->header->messageType);
                    LOGF("%smessageType: %s", _indent, eVal->name);
                }
                if (cmd->header->has_timeout) {
                    LOGF("%stimeout: %lld", _indent,
                         cmd->header->ackSequence);
                }
                if (cmd->header->has_earlyExit) {
                    LOGF("%searlyExit: %s", _indent,
                         cmd->header->earlyExit ? _str_true : _str_false);
                }
                if (cmd->header->has_priority) {
                    const ProtobufCEnumValue* eVal = 
                        protobuf_c_enum_descriptor_get_value(
                            &KineticProto_command_priority__descriptor,
                            cmd->header->messageType);
                    LOGF("%spriority: %s", _indent, eVal->name);
                }
                if (cmd->header->has_TimeQuanta) {
                    LOGF("%sTimeQuanta: %s", _indent,
                         cmd->header->TimeQuanta ? _str_true : _str_false);
                }
            }
            LOG_PROTO_LEVEL_END();
        #endif
        }

        if (cmd->body) {
            LOG_PROTO_LEVEL_START("body");
            {
                if (cmd->body->keyValue) {
                #if USE_GENERIC_LOGGER
                        KineticLogger_LogProtobufMessage((ProtobufCMessage*)cmd->body->keyValue, _indent);
                #else
                    LOG_PROTO_LEVEL_START("keyValue");
                    {
                        if (cmd->body->keyValue->has_key) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->key, 0,
                                             cmd->body->keyValue->key.len);
                            LOGF("%skey: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_newVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->newVersion,
                                             0, cmd->body->keyValue->newVersion.len);
                            LOGF("%snewVersion: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_dbVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->dbVersion,
                                             0, cmd->body->keyValue->dbVersion.len);
                            LOGF("%sdbVersion: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_tag) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->tag,
                                             0, cmd->body->keyValue->tag.len);
                            LOGF("%stag: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_force) {
                            LOGF("%sforce: %s", _indent,
                                 cmd->body->keyValue->force ? _str_true : _str_false);
                        }
                        if (cmd->body->keyValue->has_algorithm) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_command_algorithm__descriptor,
                                                                 cmd->body->keyValue->algorithm);
                            LOGF("%salgorithm: %s", _indent, eVal->name);
                        }
                        if (cmd->body->keyValue->has_metadataOnly) {
                            LOGF("%smetadataOnly: %s", _indent,
                                 cmd->body->keyValue->metadataOnly ? _str_true : _str_false);
                        }
                        if (cmd->body->keyValue->has_synchronization) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_command_synchronization__descriptor,
                                                                 cmd->body->keyValue->synchronization);
                            LOGF("%ssynchronization: %s", _indent, eVal->name);
                        }
                    }
                    LOG_PROTO_LEVEL_END();
                #endif
                }

                if (cmd->body->range) {
                #if USE_GENERIC_LOGGER
                    KineticLogger_LogProtobufMessage((ProtobufCMessage*)cmd->body->range, _indent);
                #else
                    LOG_PROTO_LEVEL_START("keyRange");
                    {
                        if (cmd->body->range->has_startKey) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->range->startKey,
                                             0, cmd->body->range->startKey.len);
                            LOGF("%sstartKey: '%s'", _indent, tmpBuf);
                        }

                        if (cmd->body->range->has_endKey) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->range->endKey,
                                             0, cmd->body->range->endKey.len);
                            LOGF("%sendKey:   '%s'", _indent, tmpBuf);
                        }

                        if (cmd->body->range->has_startKeyInclusive) {
                            LOGF("%sstartKeyInclusive: %s", _indent,
                                 cmd->body->range->startKeyInclusive ? _str_true : _str_false);
                        }

                        if (cmd->body->range->has_endKeyInclusive) {
                            LOGF("%sendKeyInclusive: %s", _indent,
                                 cmd->body->range->endKeyInclusive ? _str_true : _str_false);
                        }

                        if (cmd->body->range->has_maxReturned) {
                            LOGF("%smaxReturned: %d", _indent, cmd->body->range->maxReturned);
                        }

                        if (cmd->body->range->has_reverse) {
                            LOGF("%sreverse: %s", _indent,
                                 cmd->body->range->reverse ? _str_true : _str_false);
                        }

                        if (cmd->body->range->n_keys == 0 || cmd->body->range->keys == NULL) {
                            LOGF("%skeys: NONE", _indent);
                        }
                        else {
                            LOGF("%skeys: %d", _indent, cmd->body->range->n_keys);
                            // LOGF("XXXX BYTE_ARRAYS: range=0x%0llX, keys=0x%0llX, count=%u",
                            //     cmd->body->range, cmd->body->range->keys, cmd->body->range->n_keys);
                            for (unsigned int j = 0; j < cmd->body->range->n_keys; j++) {
                                // LOGF("XXXX  w/ key[%u] @ 0x%0llX", j, &cmd->body->range->keys[j]);
                                BYTES_TO_CSTRING(tmpBuf,
                                                 cmd->body->range->keys[j],
                                                 0, cmd->body->range->keys[j].len);
                                LOGF("%skeys[%d]: '%s'", _indent, j, tmpBuf);
                            }
                        }
                    }
                    LOG_PROTO_LEVEL_END();
                #endif
                }
            }

            LOG_PROTO_LEVEL_END();
        }

        if (cmd->status) {
        // #if USE_GENERIC_LOGGER
        //     KineticLogger_LogProtobufMessage((ProtobufCMessage*)cmd->status, _indent);
        // #else
            LOG_PROTO_LEVEL_START("status");
            {
                if (cmd->status->has_code) {
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                         &KineticProto_command_status_status_code__descriptor,
                                                         cmd->status->code);
                    LOGF("%scode: %s", _indent, eVal->name);
                }
        
                if (cmd->status->statusMessage) {
                    LOGF("%sstatusMessage: '%s'", _indent, cmd->status->statusMessage);
                }
                if (cmd->status->has_detailedMessage) {
                    BYTES_TO_CSTRING(tmpBuf,
                                     cmd->status->detailedMessage,
                                     0, cmd->status->detailedMessage.len);
                    LOGF("%sdetailedMessage: '%s'", _indent, tmpBuf);
                }
            }
            LOG_PROTO_LEVEL_END();
        // #endif
        }

        free(cmd);

        LOG_PROTO_LEVEL_END();
    }
}

void KineticLogger_LogStatus(KineticProto_Command_Status* status)
{
    if (LogLevel < 0) {
        return;
    }

    ProtobufCMessage* protoMessage = (ProtobufCMessage*)status;
    KineticProto_Command_Status_StatusCode code = status->code;

    if (code == KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS) {
        printf("Operation completed successfully\n");
    }
    else if (code == KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_STATUS_CODE) {
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
    if (LogLevel < 0) {
        return;
    }

    assert(title != NULL);
    if (bytes.data == NULL) {
        LOGF("%s: (??? bytes : buffer is NULL)", title);
        return;
    }
    if (bytes.data == NULL) {
        LOGF("%s: (0 bytes)", title);
        return;
    }
    LOGF("%s: (%zd bytes)", title, bytes.len);
    const int byteChars = 4;
    const int bytesPerLine = 32;
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

void KineticLogger_LogByteBuffer(const char* title, ByteBuffer buffer)
{
    ByteArray array = {.data = buffer.array.data, .len = buffer.bytesUsed};
    KineticLogger_LogByteArray(title, array);
}
