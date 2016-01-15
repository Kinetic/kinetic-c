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

#include "kinetic_logger.h"
#include "kinetic_version_info.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BUFFER_MAX_STRLEN (BUFFER_SIZE-2)

STATIC int KineticLogLevel = -1;
static FILE* KineticLoggerHandle = NULL;
static pthread_mutex_t BufferMutex = PTHREAD_MUTEX_INITIALIZER;
static char Buffer[BUFFER_SIZE];

//------------------------------------------------------------------------------
// Private Method Declarations

static inline bool is_level_enabled(int log_level);
static inline void lock_buffer(void);
static inline void unlock_buffer(void);
static void flush_buffer(void);
static inline char* get_buffer(void);
static inline void finish_buffer(void);
static void log_protobuf_message(int log_level, const ProtobufCMessage *msg, char* indent);
static void log_version_info(void);

//------------------------------------------------------------------------------
// Public Method Definitions

void KineticLogger_Init(const char* log_file, int log_level)
{
    KineticLogLevel = -1;
    pthread_mutex_init(&BufferMutex, NULL);

    KineticLoggerHandle = NULL;
    if (log_file == NULL) {
        printf("\nLogging kinetic-c output is disabled!\n");
        return;
    }
    else {
        KineticLogLevel = log_level;

        if (strncmp(log_file, "stdout", 4) == 0 || strncmp(log_file, "STDOUT", 4) == 0) {
            if (log_level > 0) {
                printf("Logging kinetic-c output to console (stdout) w/ log_level=%d\n", KineticLogLevel);
            }
            KineticLoggerHandle = stdout;
        }
        else {
            if (log_level > 0) {
                printf("Logging kinetic-c output to %s w/ log_level=%d\n", log_file, KineticLogLevel);
            }
            KineticLoggerHandle = fopen(log_file, "a+");
            KINETIC_ASSERT(KineticLoggerHandle != NULL);
        }
        log_version_info();
    }
}

void KineticLogger_Close(void)
{
    if (KineticLogLevel >= 0 && KineticLoggerHandle != NULL) {
        if (KineticLoggerHandle != stdout) {
            fclose(KineticLoggerHandle);
        }
        KineticLoggerHandle = NULL;
    }
}

void KineticLogger_Log(int log_level, const char* message)
{
    KineticLogger_LogPrintf(log_level, "%s", message);
}

void KineticLogger_LogPrintf(int log_level, const char* format, ...)
{
    if (KineticLoggerHandle == NULL) {
        return;
    }
    if (format == NULL || !is_level_enabled(log_level)) {
        return;
    }

    char* buffer = NULL;
    buffer = get_buffer();

    /* Add timestamp prefix */
    struct timeval tv;
    if (0 == gettimeofday(&tv, NULL)) {
        fprintf(KineticLoggerHandle, "%08lld.%06lld  ",
            (long long)tv.tv_sec, (long long)tv.tv_usec);
    }

    va_list arg_ptr;
    va_start(arg_ptr, format);
    int len = vsnprintf(buffer, BUFFER_MAX_STRLEN, format, arg_ptr);
    va_end(arg_ptr);

    if (len > BUFFER_MAX_STRLEN) {
        Buffer[BUFFER_MAX_STRLEN] = '\0';
    }
    strcat(Buffer, "\n");

    finish_buffer();
}

static void log_version_info(void)
{
    KineticLogger_LogPrintf(1, "kinetic-c version: v%s (protocol: v%s, commit: %s)",
        KINETIC_C_VERSION, KINETIC_C_PROTOCOL_VERSION, KINETIC_C_REPO_HASH);
}

void KineticLogger_LogLocation(const char* filename, int line, const char* message)
{
    if (filename == NULL || message == NULL) {
        return;
    }

    if (KineticLogLevel >= 0) {
        KineticLogger_LogPrintf(1, "[@%s:%d] %s", filename, line, message);
    }
    else {
        printf("\n[@%s:%d] %s\n", filename, line, message);
        fflush(stdout);
    }
}

void KineticLogger_LogHeader(int log_level, const KineticPDUHeader* header)
{
    if (header == NULL || !is_level_enabled(log_level)) {
        return;
    }

    KineticLogger_Log(log_level, "PDU Header:");
    KineticLogger_LogPrintf(log_level, "  versionPrefix: %c", header->versionPrefix);
    KineticLogger_LogPrintf(log_level, "  protobufLength: %d", header->protobufLength);
    KineticLogger_LogPrintf(log_level, "  valueLength: %d", header->valueLength);
}


// Helper macros and stuff for logging protobufs
#define LOG_INDENT "  "
static char indent[64] = LOG_INDENT;
static const size_t max_indent = sizeof(indent)-3;
static int indent_overflow = 0;


static void log_proto_level_start(const char* name)
{
    KineticLogger_LogPrintf(0, "%s%s {", indent, name); \
    if (strlen(indent) < max_indent) {
        strcat(indent, LOG_INDENT);
    }
    else {
        indent_overflow++;
    }
}

static void log_proto_level_end(void)
{
    if (indent_overflow == 0) {
        indent[strlen(indent) - 2] = '\0';
    }
    else {
        indent_overflow--;
    }
    KineticLogger_LogPrintf(0, "%s}", indent);
}

static void log_proto_level_start_array(const char* name, unsigned quantity)
{
    KineticLogger_LogPrintf(0, "%s%s: (%u elements)", indent, name, quantity);
    KineticLogger_LogPrintf(0, "%s[", (indent));
    if (strlen(indent) < max_indent) {
        strcat(indent, LOG_INDENT);
    }
    else {
        indent_overflow++;
    }
}

static void log_proto_level_end_array(void)
{
    if (indent_overflow == 0) {
        indent[strlen(indent) - 2] = '\0';
    }
    else {
        indent_overflow--;
    }
    KineticLogger_LogPrintf(0, "%s]", indent);
}

static int bytetoa(char* p_buf, uint8_t val)
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
        len += bytetoa(&p_buf[len], bytes.data[start + i]);
    }
    p_buf[len] = '\0';
    return len;
}

#define BOOL_TO_STRING(_bool) (_bool) ? "true" : "false"

static void LogUnboxed(int log_level,
                void const * const fieldData,
                ProtobufCFieldDescriptor const * const fieldDesc,
                size_t const i,
                char* log_indent)
{
    switch (fieldDesc->type) {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
        {
            int32_t const * value = (int32_t const *)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %ld", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
        {
            int64_t* value = (int64_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %lld", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
        {
            uint32_t* value = (uint32_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %lu", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
        {
            uint64_t* value = (uint64_t*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %llu", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_FLOAT:
        {
            float* value = (float*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %f", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_DOUBLE:
        {
            double* value = (double*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %f", log_indent, fieldDesc->name, value[i]);
        }
        break;

    case PROTOBUF_C_TYPE_BOOL:
        {
            protobuf_c_boolean* value = (protobuf_c_boolean*)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %s", log_indent, fieldDesc->name, BOOL_TO_STRING(value[i]));
        }
        break;

    case PROTOBUF_C_TYPE_STRING:
        {
            char** strings = (char**)fieldData;
            KineticLogger_LogPrintf(log_level, "%s%s: %s", log_indent, fieldDesc->name, strings[i]);
        }
        break;

    case PROTOBUF_C_TYPE_BYTES:
        {
            ProtobufCBinaryData* value = (ProtobufCBinaryData*)fieldData;
            log_proto_level_start(fieldDesc->name);
            KineticLogger_LogByteArray(log_level, log_indent,
                                       (ByteArray){.data = value[i].data,
                                                   .len = value[i].len});
            log_proto_level_end();
        }
        break;

    case PROTOBUF_C_TYPE_ENUM:
        {
            int * value = (int*)fieldData;
            ProtobufCEnumDescriptor const * enumDesc = fieldDesc->descriptor;
            ProtobufCEnumValue const * enumVal = protobuf_c_enum_descriptor_get_value(enumDesc, value[i]);
            KineticLogger_LogPrintf(log_level, "%s%s: %s", log_indent, fieldDesc->name, enumVal->name);
        }
        break;

    case PROTOBUF_C_TYPE_MESSAGE:  // nested message
        {
            ProtobufCMessage** msg = (ProtobufCMessage**)fieldData;
            if (msg[i] != NULL)
            {
                log_proto_level_start(fieldDesc->name);
                log_protobuf_message(log_level, msg[i], log_indent);
                log_proto_level_end();
            }
        } break;

    default:
        KineticLogger_LogPrintf(log_level, "Invalid message field type!: %d", fieldDesc->type);
        KINETIC_ASSERT(false); // should never get here!
        break;
    };
}

static void log_protobuf_message(int log_level, ProtobufCMessage const * msg, char* log_indent)
{
    if (msg == NULL || msg->descriptor == NULL || !is_level_enabled(log_level)) {
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
                LogUnboxed(log_level, &pMsg[fieldDesc->offset], fieldDesc, 0, log_indent);
            } break;
            case PROTOBUF_C_LABEL_OPTIONAL:
            {
                protobuf_c_boolean const * quantifier = (protobuf_c_boolean const *)(void*)&pMsg[fieldDesc->quantifier_offset];
                if ((*quantifier) &&  // only print out if it's there
                    // and a special case: if this is a message, don't show it if the message is NULL
                    (PROTOBUF_C_TYPE_MESSAGE != fieldDesc->type || ((ProtobufCMessage**)(void*)&pMsg[fieldDesc->offset])[0] != NULL))
                {
                    // special case for nested command packed into commandBytes field
                    if ((protobuf_c_message_descriptor_get_field_by_name(desc, "commandBytes") == fieldDesc ) &&
                        (PROTOBUF_C_TYPE_BYTES == fieldDesc->type))
                    {
                        ProtobufCBinaryData* value = (ProtobufCBinaryData*)(void*)&pMsg[fieldDesc->offset];
                        if ((value->data != NULL) && (value->len > 0)) {
                            log_proto_level_start(fieldDesc->name);
                            Com__Seagate__Kinetic__Proto__Command * cmd = com__seagate__kinetic__proto__command__unpack(NULL, value->len, value->data);
                            log_protobuf_message(log_level, &cmd->base, log_indent);
                            log_proto_level_end();
                            com__seagate__kinetic__proto__command__free_unpacked(cmd, NULL);
                        }
                    }
                    else {
                        LogUnboxed(log_level, &pMsg[fieldDesc->offset], fieldDesc, 0, log_indent);
                    }
                }
            } break;
            case PROTOBUF_C_LABEL_REPEATED:
            {
                unsigned const * quantifier = (unsigned const *)(void*)&pMsg[fieldDesc->quantifier_offset];
                if (*quantifier > 0) {
                    log_proto_level_start_array(fieldDesc->name, *quantifier);
                    for (uint32_t j = 0; j < *quantifier; j++) {
                        void const ** box = (void const **)(void*)&pMsg[fieldDesc->offset];
                        LogUnboxed(log_level, *box, fieldDesc, j, log_indent);
                    }
                    log_proto_level_end_array();
                }
            } break;
        }
    }
}

void KineticLogger_LogProtobuf(int log_level, const Com__Seagate__Kinetic__Proto__Message* msg)
{
    if (msg == NULL || !is_level_enabled(log_level)) {
        return;
    }
    indent_overflow = 0;

    KineticLogger_Log(log_level, "Kinetic Protobuf:");

    log_protobuf_message(log_level, &msg->base, indent);
}

void KineticLogger_LogStatus(int log_level, Com__Seagate__Kinetic__Proto__Command__Status* status)
{
    if (status == NULL || !is_level_enabled(log_level)) {
        return;
    }

    ProtobufCMessage* protoMessage = &status->base;
    Com__Seagate__Kinetic__Proto__Command__Status__StatusCode code = status->code;

    if (code == COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__SUCCESS) {
        KineticLogger_LogPrintf(log_level, "Operation completed successfully\n");
    }
    else if (code == COM__SEAGATE__KINETIC__PROTO__COMMAND__STATUS__STATUS_CODE__INVALID_STATUS_CODE) {
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
        if (status->statusmessage) {
            const ProtobufCFieldDescriptor* statusMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(protoMessageDescriptor, "statusMessage");
            const ProtobufCMessageDescriptor* statusMsgDescriptor =
                (ProtobufCMessageDescriptor*)statusMsgFieldDescriptor->descriptor;

            KineticLogger_LogPrintf(log_level, "  %s: '%s'", statusMsgDescriptor->name, status->statusmessage);
        }

        // Output detailed message, if supplied
        if (status->has_detailedmessage) {
            char tmp[8], msg[256];
            const ProtobufCFieldDescriptor* statusDetailedMsgFieldDescriptor =
                protobuf_c_message_descriptor_get_field_by_name(
                    protoMessageDescriptor, "detailedMessage");
            const ProtobufCMessageDescriptor* statusDetailedMsgDescriptor =
                (ProtobufCMessageDescriptor*)
                statusDetailedMsgFieldDescriptor->descriptor;

            sprintf(msg, "  %s: ", statusDetailedMsgDescriptor->name);
            for (size_t i = 0; i < status->detailedmessage.len; i++) {
                sprintf(tmp, "%02hhX", status->detailedmessage.data[i]);
                strcat(msg, tmp);
            }
            KineticLogger_LogPrintf(log_level, "  %s", msg);
        }
    }
}

void KineticLogger_LogByteArray(int log_level, const char* title, ByteArray bytes)
{
    if (title == NULL || !is_level_enabled(log_level)) {
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
    if (title == NULL || !is_level_enabled(log_level)) {
        return;
    }
    ByteArray array = {.data = buffer.array.data, .len = buffer.bytesUsed};
    KineticLogger_LogByteArray(log_level, title, array);
}

//------------------------------------------------------------------------------
// Private Method Definitions

static inline bool is_level_enabled(int log_level)
{
    return (log_level <= KineticLogLevel && KineticLogLevel >= 0);
}

static inline void lock_buffer(void)
{
    pthread_mutex_lock(&BufferMutex);
}

static inline void unlock_buffer()
{
    pthread_mutex_unlock(&BufferMutex);
}

static void flush_buffer(void)
{
    fprintf(KineticLoggerHandle, "%s", Buffer);
    fflush(KineticLoggerHandle);
}

static inline char* get_buffer()
{
    lock_buffer();
    return Buffer;
}

static inline void finish_buffer(void)
{
    flush_buffer();
    unlock_buffer();
}
