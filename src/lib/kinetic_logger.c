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
#include <time.h>

#define KINETIC_LOGGER_BUFFER_STR_MAX_LEN 256
#define KINETIC_LOGGER_BUFFER_SIZE (0x1 << 12)
#define KINETIC_LOGGER_FLUSH_INTERVAL_SEC 180
#define KINETIC_LOGGER_SLEEP_TIME_SEC 10
#define KINETIC_LOGGER_BUFFER_FLUSH_SIZE (0.8 * KINETIC_LOGGER_BUFFER_SIZE)

STATIC int KineticLogLevel = -1;
STATIC FILE* KineticLoggerHandle = NULL;
STATIC pthread_mutex_t KineticLoggerBufferMutex = PTHREAD_MUTEX_INITIALIZER;
STATIC char KineticLoggerBuffer[KINETIC_LOGGER_BUFFER_SIZE][KINETIC_LOGGER_BUFFER_STR_MAX_LEN];
STATIC int KineticLoggerBufferSize = 0;

#if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
STATIC pthread_t KineticLoggerFlushThread;
STATIC bool KineticLoggerForceFlush = false;
STATIC bool KineticLogggerAbortRequested = false;
#else
STATIC bool KineticLoggerForceFlush = true;
#endif


//------------------------------------------------------------------------------
// Private Method Declarations

static inline bool KineticLogger_IsLevelEnabled(int log_level);
static inline void KineticLogger_BufferLock(void);
static inline void KineticLogger_BufferUnlock(void);
static void KineticLogger_FlushBuffer(void);
static inline char* KineticLogger_GetBuffer(void);
static inline void KineticLogger_FinishBuffer(void);
#if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
static void* KineticLogger_FlushThread(void* arg);
static void KineticLogger_InitFlushThread(void);
#endif
static void KineticLogger_LogProtobufMessage(int log_level, const ProtobufCMessage *msg, char* _indent);


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
        KineticLoggerForceFlush = false;
        KineticLogger_InitFlushThread();
        #endif
    }
}

void KineticLogger_Close(void)
{
    if (KineticLogLevel >= 0 && KineticLoggerHandle != NULL) {
        #if KINETIC_LOGGER_FLUSH_THREAD_ENABLED
        KineticLogggerAbortRequested = true;
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
    if (message == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }

    char* buffer = NULL;
    buffer = KineticLogger_GetBuffer();
    snprintf(buffer, KINETIC_LOGGER_BUFFER_STR_MAX_LEN, "%s\n", message);
    KineticLogger_FinishBuffer();
}

void KineticLogger_LogPrintf(int log_level, const char* format, ...)
{
    if (format == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
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
    if (header == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }

    KineticLogger_Log(log_level, "PDU Header:");
    KineticLogger_LogPrintf(log_level, "  versionPrefix: %c", header->versionPrefix);
    KineticLogger_LogPrintf(log_level, "  protobufLength: %d", header->protobufLength);
    KineticLogger_LogPrintf(log_level, "  valueLength: %d", header->valueLength);
}

#define LOG_PROTO_INIT() char _indent[32] = "  ";

#define LOG_PROTO_LEVEL_START(__name) \
    KineticLogger_LogPrintf(2, "%s%s {", (_indent), (__name)); \
    strcat(_indent, "  ");
#define LOG_PROTO_LEVEL_START_NO_INDENT() \
    KineticLogger_LogPrintf(2, "{"); \
    strcat(_indent, "  ");
#define LOG_PROTO_LEVEL_END() \
    _indent[strlen(_indent) - 2] = '\0'; \
    KineticLogger_LogPrintf(2, "%s}", _indent);

#define LOG_PROTO_LEVEL_START_ARRAY(__name, __quantity) \
    KineticLogger_LogPrintf(2, "%s%s: (%u elements) [", (_indent), (__name), (__quantity)); \
    strcat(_indent, "  ");
#define LOG_PROTO_LEVEL_END_ARRAY() \
    _indent[strlen(_indent) - 2] = '\0'; \
    KineticLogger_LogPrintf(2, "%s]", _indent);

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

static void LogUnboxed(int log_level,
                void const * const fieldData,
                ProtobufCFieldDescriptor const * const fieldDesc,
                size_t const i,
                char* _indent)
{
    switch (fieldDesc->type) {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
        {
            int32_t const * value = (int32_t const *)fieldData;
            KineticLogger_LogPrintf(log_level, "%ld", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
        {
            int64_t* value = (int64_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%lld", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
        {
            uint32_t* value = (uint32_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%lu", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
        {
            uint64_t* value = (uint64_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%llu", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_FLOAT:
        {
            float* value = (float*)fieldData;
            KineticLogger_LogPrintf(log_level, "%f", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_DOUBLE:
        {
            double* value = (double*)fieldData;
            KineticLogger_LogPrintf(log_level, "%f", value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_BOOL:
        {
            protobuf_c_boolean* value = (protobuf_c_boolean*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s", BOOL_TO_STRING(value[i]));
        }
        break;

    case PROTOBUF_C_TYPE_STRING:
        {
            char** strings = (char**)fieldData;
            KineticLogger_LogPrintf(log_level, "%s", strings[i]);
        }
        break;

    case PROTOBUF_C_TYPE_BYTES:
        {
            ProtobufCBinaryData* value = (ProtobufCBinaryData*)fieldData;
            LOG_PROTO_LEVEL_START_NO_INDENT();
            KineticLogger_LogByteArray(log_level, _indent,
                                       (ByteArray){.data = value[i].data,
                                                   .len = value[i].len});
            LOG_PROTO_LEVEL_END();
        }
        break;

    case PROTOBUF_C_TYPE_ENUM:
        {
            int * value = (int*)fieldData;
            ProtobufCEnumDescriptor const * enumDesc = fieldDesc->descriptor;
            ProtobufCEnumValue const * enumVal = protobuf_c_enum_descriptor_get_value(enumDesc, value[i]);
            KineticLogger_LogPrintf(log_level, "%s", enumVal->name);
        }
        break;

    case PROTOBUF_C_TYPE_MESSAGE:  // nested message
        {
            ProtobufCMessage** msg = (ProtobufCMessage**)fieldData;

            if (msg[i] != NULL)
            {
                LOG_PROTO_LEVEL_START_NO_INDENT();
                KineticLogger_LogProtobufMessage(log_level, msg[i], _indent);
                LOG_PROTO_LEVEL_END();
            }
        } break;

    default:
        KineticLogger_LogPrintf(log_level, "Invalid message field type!: %d", fieldDesc->type);
        assert(false); // should never get here!
        break;
    };
}

static void KineticLogger_LogProtobufMessage(int log_level, ProtobufCMessage const * msg, char* _indent)
{
    if (msg == NULL || msg->descriptor == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }

    ProtobufCMessageDescriptor const * desc = msg->descriptor;
    uint8_t const * pMsg = (uint8_t const *)msg;

    for (unsigned int i = 0; i < desc->n_fields; i++) {
        ProtobufCFieldDescriptor const * fieldDesc = &desc->fields[i];

        if (fieldDesc == NULL) {
            continue;
        }

        switch(fieldDesc->label)
        {
            case PROTOBUF_C_LABEL_REQUIRED:
            {
                printf("%s%s: ", _indent, fieldDesc->name);

                LogUnboxed(log_level, &pMsg[fieldDesc->offset], fieldDesc, 0, _indent);
            } break;
            case PROTOBUF_C_LABEL_OPTIONAL:
            {
                protobuf_c_boolean const * quantifier = (protobuf_c_boolean const *)&pMsg[fieldDesc->quantifier_offset];
                if ((*quantifier) &&  // only print out if it's there
                    // and a special case: if this is a message, don't show it if the message is NULL
                    (PROTOBUF_C_TYPE_MESSAGE != fieldDesc->type || ((ProtobufCMessage**)&pMsg[fieldDesc->offset])[0] != NULL)) 
                {
                    printf("%s%s: ", _indent, fieldDesc->name);

                    /* special case for nested command packed into commandBytes field */
                    if ((protobuf_c_message_descriptor_get_field_by_name(desc, "commandBytes") == fieldDesc ) && 
                        (PROTOBUF_C_TYPE_BYTES == fieldDesc->type)) {

                        ProtobufCBinaryData* value = (ProtobufCBinaryData*)&pMsg[fieldDesc->offset];
                        if ((value->data != NULL) && (value->len > 0)) {
                            KineticProto_Command * cmd = KineticProto_command__unpack(NULL, value->len, value->data);

                            LOG_PROTO_LEVEL_START_NO_INDENT();
                            KineticLogger_LogProtobufMessage(log_level, &cmd->base, _indent);
                            LOG_PROTO_LEVEL_END();
                            free(cmd);
                        }
                    }
                    else
                    {
                        LogUnboxed(log_level, &pMsg[fieldDesc->offset], fieldDesc, 0, _indent);
                    }
                }
            } break;
            case PROTOBUF_C_LABEL_REPEATED:
            {
                unsigned const * quantifier = (unsigned const *)&pMsg[fieldDesc->quantifier_offset];
                if (*quantifier > 0)
                {
                    LOG_PROTO_LEVEL_START_ARRAY(fieldDesc->name, *quantifier);
                    for (uint32_t i = 0; i < *quantifier; i++)
                    {
                        void const ** box = (void const **)&pMsg[fieldDesc->offset];
                        printf("%s", _indent);
                        LogUnboxed(log_level, *box, fieldDesc, i, _indent);
                    }
                    LOG_PROTO_LEVEL_END_ARRAY();
                }
            } break;
        }
    }
}

void KineticLogger_LogProtobuf(int log_level, const KineticProto_Message* msg)
{
    if (msg == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }
    LOG_PROTO_INIT();

    KineticLogger_Log(log_level, "Kinetic Protobuf:");

    KineticLogger_LogProtobufMessage(log_level, &msg->base, _indent);
}

void KineticLogger_LogStatus(int log_level, KineticProto_Command_Status* status)
{
    if (status == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }

    ProtobufCMessage* protoMessage = &status->base;
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
    if (title == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }

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
        KineticLogger_LogPrintf(log_level, "%s:  %s : %s", title, hex, ascii);
    }
}

void KineticLogger_LogByteBuffer(int log_level, const char* title, ByteBuffer buffer)
{
    if (title == NULL || !KineticLogger_IsLevelEnabled(log_level)) {
        return;
    }
    ByteArray array = {.data = buffer.array.data, .len = buffer.bytesUsed};
    KineticLogger_LogByteArray(log_level, title, array);
}


//------------------------------------------------------------------------------
// Private Method Definitions

static inline bool KineticLogger_IsLevelEnabled(int log_level)
{
    return (log_level <= KineticLogLevel && KineticLogLevel >= 0);
}

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

static bool LoggerFlushThreadStarted;

static void* KineticLogger_FlushThread(void* arg)
{
    (void)arg;
    struct timeval tv;
    time_t lasttime, curtime;

    gettimeofday(&tv, NULL);

    lasttime = tv.tv_sec;
    LoggerFlushThreadStarted = true;

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
    LoggerFlushThreadStarted = false;
    pthread_create(&KineticLoggerFlushThread, NULL, KineticLogger_FlushThread, NULL);
    KineticLogger_Log(3, "Logger flush thread started!");
    KineticLogger_FlushBuffer();
}

#endif
