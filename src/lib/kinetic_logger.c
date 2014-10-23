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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

// #define USE_GENERIC_LOGGER 1 (not ready yet!)

#define KINETIC_LOGGER_BUFFER_STR_MAX_LEN 256
#define KINETIC_LOGGER_BUFFER_SIZE (0x1 << 12)
#define KINETIC_LOGGER_FLUSH_INTERVAL_SEC 180
#define KINETIC_LOGGER_SLEEP_TIME_SEC 10
#define KINETIC_LOGGER_BUFFER_FLUSH_SIZE (0.8 * KINETIC_LOGGER_BUFFER_SIZE)

STATIC int KineticLogLevel = -1;
STATIC FILE* KineticLoggerHandle = NULL;
STATIC bool KineticLogggerAbortRequested = false;
STATIC pthread_t KineticLoggerFlushThread;
STATIC pthread_mutex_t KineticLoggerBufferMutex = PTHREAD_MUTEX_INITIALIZER;
STATIC char KineticLoggerBuffer[KINETIC_LOGGER_BUFFER_SIZE][KINETIC_LOGGER_BUFFER_STR_MAX_LEN];
STATIC int KineticLoggerBufferSize = 0;

#if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
STATIC bool KineticLoggerForceFlush = false;
#else
STATIC bool KineticLoggerForceFlush = true;
#endif


//------------------------------------------------------------------------------
// Private Method Declarations

static inline void KineticLogger_BufferLock(void);
static inline void KineticLogger_BufferUnlock(void);
static void KineticLogger_FlushBuffer(void);
static inline char* KineticLogger_GetBuffer(void);
static inline void KineticLogger_FinishBuffer(void);
#if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
static void* KineticLogger_FlushThread(void* arg);
static void KineticLogger_InitFlushThread(void);
#endif


//------------------------------------------------------------------------------
// Public Method Definitions

void KineticLogger_Init(const char* log_file, int log_level)
{
    KineticLogLevel = -1;

    KineticLoggerHandle = NULL;
    if (log_file == NULL) {
        printf("\nLogging kinetic-c output is disabled!\n");
        return;
    }
    else {
        KineticLogLevel = log_level;
        
        if (strncmp(log_file, "stdout", 4) == 0 || strncmp(log_file, "STDOUT", 4) == 0) {
            printf("\nLogging kinetic-c output to console (stdout) w/ log_level=%d\n", KineticLogLevel);
            KineticLoggerHandle = stdout;
        }
        else {
            printf("\nLogging kinetic-c output to %s w/ log_level=%d\n", log_file, KineticLogLevel);
            KineticLoggerHandle = fopen(log_file, "a+");
            assert(KineticLoggerHandle != NULL);
        }

        // Create thread to periodically flush the log
        #if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
        KineticLogger_InitFlushThread();
        KineticLoggerForceFlush = false;
        #endif
    }
}

void KineticLogger_Close(void)
{
    if (KineticLogLevel >= 0 && KineticLoggerHandle != NULL) {
        KineticLogggerAbortRequested = true;
        #if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
        int pthreadStatus = pthread_join(KineticLoggerFlushThread, NULL);
        if (pthreadStatus != 0) {
            char errMsg[256];
            Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
            LOGF0("Failed terminating logger flush thread w/error: %s", errMsg);
        }
        #endif
        KineticLogger_FlushBuffer();
        if (KineticLoggerHandle != stdout) {
            fclose(KineticLoggerHandle);
        }
        KineticLoggerHandle = NULL;
    }
}

void KineticLogger_Log(int log_level, const char* message)
{
    if (message == NULL || log_level > KineticLogLevel || KineticLogLevel < 0) {
        return;
    }

    char* buffer = NULL;
    buffer = KineticLogger_GetBuffer();
    snprintf(buffer, KINETIC_LOGGER_BUFFER_STR_MAX_LEN, "%s\n", message);
    KineticLogger_FinishBuffer();
}

void KineticLogger_LogPrintf(int log_level, const char* format, ...)
{
    if (format == NULL || log_level > KineticLogLevel || KineticLogLevel < 0) {
        return;
    }

    char* buffer = NULL;
    buffer = KineticLogger_GetBuffer();

    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsnprintf(buffer, KINETIC_LOGGER_BUFFER_STR_MAX_LEN, format, arg_ptr);
    va_end(arg_ptr);

    strcat(buffer, "\n");
    KineticLogger_FinishBuffer();
}

void KineticLogger_LogLocation(const char* filename, int line, const char* message)
{
    if (filename == NULL || message == NULL) {
        return;
    }

    if (KineticLogLevel >= 0) {
        KineticLogger_LogPrintf(1, "\n[@%s:%d] %s", filename, line, message);
    }
    else
    {
        printf("\n[@%s:%d] %s\n", filename, line, message);
        fflush(stdout);
    }
}

void KineticLogger_LogHeader(int log_level, const KineticPDUHeader* header)
{
    if (header == NULL || log_level < KineticLogLevel || KineticLogLevel < 0) {
        return;
    }

    KineticLogger_Log(log_level, "PDU Header:");
    KineticLogger_LogPrintf(log_level, "  versionPrefix: %c", header->versionPrefix);
    KineticLogger_LogPrintf(log_level, "  protobufLength: %d", header->protobufLength);
    KineticLogger_LogPrintf(log_level, "  valueLength: %d", header->valueLength);
}

static const char* _str_true = "true";
static const char* _str_false = "false";

#define LOG_PROTO_INIT() char _indent[32] = "  ";

#define LOG_PROTO_LEVEL_START(__name) \
    KineticLogger_LogPrintf(2, "%s%s {", (_indent), (__name)); \
    strcat(_indent, "  ");

#define LOG_PROTO_LEVEL_END() \
    _indent[strlen(_indent) - 2] = '\0'; \
    KineticLogger_LogPrintf(2, "%s}", _indent);

static int KineticLogger_u8toa(char* p_buf, uint8_t val)
{
    // KineticLogger_LogPrintf(log_level, "Converting byte=%02u", val);
    const uint8_t base = 16;
    const int width = 2;
    int i = width;
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

static int KineticLogger_ByteArraySliceToCString(char* p_buf,
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
//     KineticLogger_LogByteArray(1, #attr, (el)->(attr));
// }

#if USE_GENERIC_LOGGER
static void KineticLogger_LogProtobufMessage(int log_level, const ProtobufCMessage *msg, char* _indent)
{
    if (msg == NULL || msg->descriptor == NULL || log_level < 0 || KineticLogLevel < 0) {
        return;
    }

    const ProtobufCMessageDescriptor* desc = msg->descriptor;
    const uint8_t* pMsg = (const uint8_t*)msg;
    // char tmpBuf[1024];

    LOG_PROTO_LEVEL_START(desc->short_name);
    // KineticLogger_LogPrintf(log_level, "%s* ProtobufMessage: msg=0x%0llX, _indent, range=0x%0llX, name=%s, fields=%u",
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

        // KineticLogger_LogPrintf(log_level, "%s * ProtobufField[%s]: descriptor=0x%0llX",
        //     _indent, fieldDesc->name, fieldDesc);
        // KineticLogger_LogPrintf(log_level, "%s   * value: field=0x%0llX, offset=%u, value=?, type=%d",
        //     _indent, pVal, fieldDesc->offset, fieldDesc->type);
        // KineticLogger_LogPrintf(log_level, "%s   * quantifier: quantifier=0x%0llX, offset=%u, quantity=%u",
        //     _indent, quantifier, fieldDesc->quantifier_offset, quantity);

        switch (fieldDesc->type) {
        case PROTOBUF_C_TYPE_INT32:
        case PROTOBUF_C_TYPE_SINT32:
        case PROTOBUF_C_TYPE_SFIXED32:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                int32_t* value = (int32_t*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %ld", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_INT64:
        case PROTOBUF_C_TYPE_SINT64:
        case PROTOBUF_C_TYPE_SFIXED64:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                int64_t* value = (int64_t*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %lld", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_UINT32:
        case PROTOBUF_C_TYPE_FIXED32:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                uint32_t* value = (uint32_t*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %lu", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_UINT64:
        case PROTOBUF_C_TYPE_FIXED64:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                uint64_t* value = (uint64_t*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %llu", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_FLOAT:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                float* value = (float*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %f", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_DOUBLE:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                double* value = (double*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %f", _indent, fieldDesc->name, suffix, value[i]);
            }
            break;

        case PROTOBUF_C_TYPE_BOOL:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                protobuf_c_boolean* value = (protobuf_c_boolean*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %s", _indent, fieldDesc->name, suffix,
                        value[i] ? _str_true : _str_false);
            }
            break;

        case PROTOBUF_C_TYPE_STRING:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                char** strings = (char**)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %s", _indent, fieldDesc->name, suffix, strings[i]);
            }
            break;

        case PROTOBUF_C_TYPE_BYTES:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                ProtobufCBinaryData* array = (ProtobufCBinaryData*)(msg + fieldDesc->offset + (i * sizeof(ProtobufCBinaryData)));
                KineticLogger_LogPrintf(log_level, "%s%s%s: addr=0x%0llX, data=0x%0llX, len=%zu",
                    _indent, fieldDesc->name, suffix, array, array->data, array->len);
            }
            break;

            // if (quantity > 0) {
            //     KineticLogger_LogPrintf(log_level, "%s* TYPE: PROTOBUF_C_TYPE_BYTES (incomplete!)", _indent);
            //     // ProtobufCBinaryData* pArray = *(ProtobufCBinaryData**)((unsigned long)(pVal));
            //     // KineticLogger_LogPrintf(log_level, "%s    * pArray=0x%0llX, points to: 0x%0llX", _indent, pArray, *pArray);
            //     // ProtobufCBinaryData* arrays = (ProtobufCBinaryData*)pArray;
            //     // // arrays++;
            //     // KineticLogger_LogPrintf(log_level, "%s    * BYTE_ARRAYS: raw=0x%0llX, arrays=0x%0llX, quantity=%u, &[0]=0x%0llX, &[1]=0x%0llX",
            //     //     _indent, pVal, arrays, quantity, &arrays[0], &arrays[1]);
            //     // for (unsigned int i = 0; i < quantity; i++) {
            //     //     ProtobufCBinaryData* myArray = (ProtobufCBinaryData*)&arrays[i];
            //     //     myArray += i;
            //     //     KineticLogger_LogPrintf(log_level, "%s     * data=0x%0llX, len=%u", _indent, myArray->data, myArray->len);
            //     //     if (myArray->data != NULL && myArray->len > 0) {
            //     //         char suffix[32] = {'\0'};
            //     //         if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
            //     //         // BYTES_TO_CSTRING(tmpBuf, &myArray, 0, myArray->len);
            //     //         // KineticLogger_LogPrintf(log_level, "%s%s%s: %s", _indent, fieldDesc->name, suffix, tmpBuf);
            //     //     }
            //     // }
            // }
            // break;

        case PROTOBUF_C_TYPE_ENUM:
            for (unsigned int i = 0; i < quantity; i++) {
                char suffix[32] = {'\0'};
                if (indexed) {snprintf(suffix, sizeof(suffix), "[%u]", i);}
                ProtobufCEnumValue* enumVal = (ProtobufCEnumValue*)(msg + fieldDesc->offset);
                KineticLogger_LogPrintf(log_level, "%s%s%s: %s", _indent, fieldDesc->name, suffix, enumVal[i].name);
            }
            break;

        case PROTOBUF_C_TYPE_MESSAGE:  // nested message
            KineticLogger_Log(log_level, "Nested messages not yet supported!");
            assert(false); // nested messages not yet handled!
            break;

        default:
            KineticLogger_Log(log_level, "Invalid message field type!");
            assert(false); // should never get here!
            break;
        };
    }

    LOG_PROTO_LEVEL_END();
}
#endif

void KineticLogger_LogProtobuf(int log_level, const KineticProto_Message* msg)
{
    if (log_level < KineticLogLevel || KineticLogLevel < 0 || msg == NULL) {
        return;
    }

    LOG_PROTO_INIT();
// #if USE_GENERIC_LOGGER
// #else
    char tmpBuf[1024];
// #endif

    KineticLogger_Log(log_level, "Kinetic Protobuf:");

    if (msg->has_authType) {
        const ProtobufCEnumValue* eVal = 
            protobuf_c_enum_descriptor_get_value(
                &KineticProto_Message_auth_type__descriptor,
                msg->authType);
        KineticLogger_LogPrintf(log_level, "%sauthType: %s", _indent, eVal->name);

        if (msg->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH && msg->hmacAuth != NULL) {
        #if USE_GENERIC_LOGGER
            KineticLogger_LogProtobufMessage(log_level, (ProtobufCMessage*)msg->hmacAuth, _indent);
        #else
            LOG_PROTO_LEVEL_START("hmacAuth");
            if (msg->hmacAuth->has_identity) {
                KineticLogger_LogPrintf(log_level, "%sidentity: %lld", _indent, msg->hmacAuth->identity);
            }

            if (msg->hmacAuth->has_hmac) {
                BYTES_TO_CSTRING(tmpBuf, msg->hmacAuth->hmac, 0, msg->hmacAuth->hmac.len);
                KineticLogger_LogPrintf(log_level, "%shmac: '%s'", _indent, tmpBuf);
            }

            LOG_PROTO_LEVEL_END();
        #endif
        }
        else if (msg->authType == KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH && msg->pinAuth != NULL) {
        #if USE_GENERIC_LOGGER
            KineticLogger_LogProtobufMessage(log_level, (ProtobufCMessage*)msg->hmacAuth, _indent);
        #else
            LOG_PROTO_LEVEL_START("pinAuth");

            if (msg->pinAuth->has_pin) {
                BYTES_TO_CSTRING(tmpBuf, msg->pinAuth->pin, 0, msg->pinAuth->pin.len);
                KineticLogger_LogPrintf(log_level, "%spin: '%s'", _indent, tmpBuf);
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
                    KineticLogger_LogPrintf(log_level, "%sclusterVersion: %lld", _indent,
                         cmd->header->clusterVersion);
                }
                if (cmd->header->has_connectionID) {
                    KineticLogger_LogPrintf(log_level, "%sconnectionID: %lld", _indent,
                         cmd->header->connectionID);
                }
                if (cmd->header->has_sequence) {
                    KineticLogger_LogPrintf(log_level, "%ssequence: %lld", _indent,
                         cmd->header->sequence);
                }
                if (cmd->header->has_ackSequence) {
                    KineticLogger_LogPrintf(log_level, "%sackSequence: %lld", _indent,
                         cmd->header->ackSequence);
                }
                if (cmd->header->has_messageType) {
                    const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                         &KineticProto_command_message_type__descriptor,
                                                         cmd->header->messageType);
                    KineticLogger_LogPrintf(log_level, "%smessageType: %s", _indent, eVal->name);
                }
                if (cmd->header->has_timeout) {
                    KineticLogger_LogPrintf(log_level, "%stimeout: %lld", _indent,
                         cmd->header->ackSequence);
                }
                if (cmd->header->has_earlyExit) {
                    KineticLogger_LogPrintf(log_level, "%searlyExit: %s", _indent,
                         cmd->header->earlyExit ? _str_true : _str_false);
                }
                if (cmd->header->has_priority) {
                    const ProtobufCEnumValue* eVal = 
                        protobuf_c_enum_descriptor_get_value(
                            &KineticProto_command_priority__descriptor,
                            cmd->header->messageType);
                    KineticLogger_LogPrintf(log_level, "%spriority: %s", _indent, eVal->name);
                }
                if (cmd->header->has_TimeQuanta) {
                    KineticLogger_LogPrintf(log_level, "%sTimeQuanta: %s", _indent,
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
                            KineticLogger_LogPrintf(log_level, "%skey: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_newVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->newVersion,
                                             0, cmd->body->keyValue->newVersion.len);
                            KineticLogger_LogPrintf(log_level, "%snewVersion: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_dbVersion) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->dbVersion,
                                             0, cmd->body->keyValue->dbVersion.len);
                            KineticLogger_LogPrintf(log_level, "%sdbVersion: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_tag) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->keyValue->tag,
                                             0, cmd->body->keyValue->tag.len);
                            KineticLogger_LogPrintf(log_level, "%stag: '%s'", _indent, tmpBuf);
                        }
                        if (cmd->body->keyValue->has_force) {
                            KineticLogger_LogPrintf(log_level, "%sforce: %s", _indent,
                                 cmd->body->keyValue->force ? _str_true : _str_false);
                        }
                        if (cmd->body->keyValue->has_algorithm) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_command_algorithm__descriptor,
                                                                 cmd->body->keyValue->algorithm);
                            KineticLogger_LogPrintf(log_level, "%salgorithm: %s", _indent, eVal->name);
                        }
                        if (cmd->body->keyValue->has_metadataOnly) {
                            KineticLogger_LogPrintf(log_level, "%smetadataOnly: %s", _indent,
                                 cmd->body->keyValue->metadataOnly ? _str_true : _str_false);
                        }
                        if (cmd->body->keyValue->has_synchronization) {
                            const ProtobufCEnumValue* eVal = protobuf_c_enum_descriptor_get_value(
                                                                 &KineticProto_command_synchronization__descriptor,
                                                                 cmd->body->keyValue->synchronization);
                            KineticLogger_LogPrintf(log_level, "%ssynchronization: %s", _indent, eVal->name);
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
                            KineticLogger_LogPrintf(log_level, "%sstartKey: '%s'", _indent, tmpBuf);
                        }

                        if (cmd->body->range->has_endKey) {
                            BYTES_TO_CSTRING(tmpBuf,
                                             cmd->body->range->endKey,
                                             0, cmd->body->range->endKey.len);
                            KineticLogger_LogPrintf(log_level, "%sendKey:   '%s'", _indent, tmpBuf);
                        }

                        if (cmd->body->range->has_startKeyInclusive) {
                            KineticLogger_LogPrintf(log_level, "%sstartKeyInclusive: %s", _indent,
                                 cmd->body->range->startKeyInclusive ? _str_true : _str_false);
                        }

                        if (cmd->body->range->has_endKeyInclusive) {
                            KineticLogger_LogPrintf(log_level, "%sendKeyInclusive: %s", _indent,
                                 cmd->body->range->endKeyInclusive ? _str_true : _str_false);
                        }

                        if (cmd->body->range->has_maxReturned) {
                            KineticLogger_LogPrintf(log_level, "%smaxReturned: %d", _indent, cmd->body->range->maxReturned);
                        }

                        if (cmd->body->range->has_reverse) {
                            KineticLogger_LogPrintf(log_level, "%sreverse: %s", _indent,
                                 cmd->body->range->reverse ? _str_true : _str_false);
                        }

                        if (cmd->body->range->n_keys == 0 || cmd->body->range->keys == NULL) {
                            KineticLogger_LogPrintf(log_level, "%skeys: NONE", _indent);
                        }
                        else {
                            KineticLogger_LogPrintf(log_level, "%skeys: %d", _indent, cmd->body->range->n_keys);
                            // KineticLogger_LogPrintf(log_level, "XXXX BYTE_ARRAYS: range=0x%0llX, keys=0x%0llX, count=%u",
                            //     cmd->body->range, cmd->body->range->keys, cmd->body->range->n_keys);
                            for (unsigned int j = 0; j < cmd->body->range->n_keys; j++) {
                                // KineticLogger_LogPrintf(log_level, "XXXX  w/ key[%u] @ 0x%0llX", j, &cmd->body->range->keys[j]);
                                BYTES_TO_CSTRING(tmpBuf,
                                                 cmd->body->range->keys[j],
                                                 0, cmd->body->range->keys[j].len);
                                KineticLogger_LogPrintf(log_level, "%skeys[%d]: '%s'", _indent, j, tmpBuf);
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
                    KineticLogger_LogPrintf(log_level, "%scode: %s", _indent, eVal->name);
                }
        
                if (cmd->status->statusMessage) {
                    KineticLogger_LogPrintf(log_level, "%sstatusMessage: '%s'", _indent, cmd->status->statusMessage);
                }
                if (cmd->status->has_detailedMessage) {
                    BYTES_TO_CSTRING(tmpBuf,
                                     cmd->status->detailedMessage,
                                     0, cmd->status->detailedMessage.len);
                    KineticLogger_LogPrintf(log_level, "%sdetailedMessage: '%s'", _indent, tmpBuf);
                }
            }
            LOG_PROTO_LEVEL_END();
        // #endif
        }

        free(cmd);

        LOG_PROTO_LEVEL_END();
    }
}

void KineticLogger_LogStatus(int log_level, KineticProto_Command_Status* status)
{
    if (status == NULL || log_level < KineticLogLevel || KineticLogLevel < 0) {
        return;
    }

    ProtobufCMessage* protoMessage = (ProtobufCMessage*)status;
    KineticProto_Command_Status_StatusCode code = status->code;

    if (code == KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_SUCCESS) {
        KineticLogger_LogPrintf(log_level, "Operation completed successfully\n");
    }
    else if (code == KINETIC_PROTO_COMMAND_STATUS_STATUS_CODE_INVALID_STATUS_CODE) {
        KineticLogger_LogPrintf(log_level, "Operation was aborted!\n");
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
        KineticLogger_LogPrintf(log_level, "Operation completed but failed w/error: %s=%d(%s)\n",
                                statusCodeDescriptor->name, code, eStatusCodeVal->name);

        // Output status message, if supplied
        if (status->statusMessage) {
            const ProtobufCFieldDescriptor* statusMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "statusMessage");
            const ProtobufCMessageDescriptor* statusMsgDescriptor =
                (ProtobufCMessageDescriptor*)statusMsgFieldDescriptor->descriptor;

            KineticLogger_LogPrintf(log_level, "  %s: '%s'", statusMsgDescriptor->name, status->statusMessage);
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
            KineticLogger_LogPrintf(log_level, "  %s", msg);
        }
    }
}

void KineticLogger_LogByteArray(int log_level, const char* title, ByteArray bytes)
{
    if (log_level < KineticLogLevel || KineticLogLevel < 0) {
        return;
    }

    assert(title != NULL);
    if (bytes.data == NULL) {
        KineticLogger_LogPrintf(log_level, "%s: (??? bytes : buffer is NULL)", title);
        return;
    }
    if (bytes.data == NULL) {
        KineticLogger_LogPrintf(log_level, "%s: (0 bytes)", title);
        return;
    }
    KineticLogger_LogPrintf(log_level, "%s: (%zd bytes)", title, bytes.len);
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
        KineticLogger_LogPrintf(log_level, "  %s : %s", hex, ascii);
    }
}

void KineticLogger_LogByteBuffer(int log_level, const char* title, ByteBuffer buffer)
{
    if (log_level < KineticLogLevel || KineticLogLevel < 0) {
        return;
    }
    ByteArray array = {.data = buffer.array.data, .len = buffer.bytesUsed};
    KineticLogger_LogByteArray(log_level, title, array);
}


//------------------------------------------------------------------------------
// Private Method Definitions

static inline void KineticLogger_BufferLock(void)
{
    pthread_mutex_lock(&KineticLoggerBufferMutex);
}

static inline void KineticLogger_BufferUnlock()
{
    pthread_mutex_unlock(&KineticLoggerBufferMutex);
}

static void KineticLogger_FlushBuffer(void)
{
    if (KineticLoggerHandle == NULL) {
        return;
    }
    for (int i = 0; i < KineticLoggerBufferSize; i++) {
        fprintf(KineticLoggerHandle, "%s", KineticLoggerBuffer[i]);
    }
    fflush(KineticLoggerHandle);
    KineticLoggerBufferSize = 0;
}

static inline char* KineticLogger_GetBuffer()
{
    KineticLogger_BufferLock();
    if (KineticLoggerBufferSize >= KINETIC_LOGGER_BUFFER_SIZE) {
        KineticLogger_FlushBuffer();
    }

    // allocate buffer
    KineticLoggerBufferSize++;
    return KineticLoggerBuffer[KineticLoggerBufferSize-1];
}

static inline void KineticLogger_FinishBuffer(void)
{
    if (KineticLoggerForceFlush) {
        KineticLogger_FlushBuffer();
    }
    KineticLogger_BufferUnlock();
}

#if KINETIC_LOGGER_FLUSH_THREAD_ENABLED

static void* KineticLogger_FlushThread(void* arg)
{
    (void)arg;
    struct timeval tv;
    time_t lasttime;
    time_t curtime;

    gettimeofday(&tv, NULL);

    lasttime = tv.tv_sec;

    while(!KineticLogggerAbortRequested) {
        sleep(KINETIC_LOGGER_SLEEP_TIME_SEC);
        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;
        if ((curtime - lasttime) >= KINETIC_LOGGER_FLUSH_INTERVAL_SEC) {
            KineticLogger_FlushBuffer();
            lasttime = curtime;
        }
        else {
            KineticLogger_BufferLock();
            if (KineticLoggerBufferSize >= KINETIC_LOGGER_BUFFER_FLUSH_SIZE) {
                KineticLogger_FlushBuffer();
            }
            KineticLogger_BufferUnlock();
        }
    }

    return NULL;
}

static void KineticLogger_InitFlushThread(void)
{
    KineticLogger_Log(3, "Starting log flush thread");
    KineticLogger_FlushBuffer();
    pthread_create(&KineticLoggerFlushThread, NULL, KineticLogger_FlushThread, NULL);
}

#endif
