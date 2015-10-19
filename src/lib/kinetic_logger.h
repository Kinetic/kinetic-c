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

#ifndef _KINETIC_LOGGER_H
#define _KINETIC_LOGGER_H

#include "kinetic_types_internal.h"
#include "kinetic.pb-c.h"
#include <stdarg.h>

#define KINETIC_LOGGER_DISABLED false
#define KINETIC_LOGGER_FLUSH_THREAD_ENABLED false
#define KINETIC_LOGGER_LOG_SEQUENCE_ID true
#define KINETIC_LOG_FILE "kinetic.log"

void KineticLogger_Init(const char* logFile, int log_level);
void KineticLogger_Close(void);
void KineticLogger_Log(int log_level, const char* message);
void KineticLogger_LogPrintf(int log_level, const char* format, ...);
void KineticLogger_LogLocation(const char* filename, int line, const char * message);
void KineticLogger_LogHeader(int log_level, const KineticPDUHeader* header);
void KineticLogger_LogProtobuf(int log_level, const Com__Seagate__Kinetic__Proto__Message* msg);
void KineticLogger_LogStatus(int log_level, Com__Seagate__Kinetic__Proto__Command__Status* status);
void KineticLogger_LogByteArray(int log_level, const char* title, ByteArray bytes);
void KineticLogger_LogByteBuffer(int log_level, const char* title, ByteBuffer buffer);

int KineticLogger_ByteArraySliceToCString(char* p_buf, const ByteArray bytes, const int start, const int count);
#define BYTES_TO_CSTRING(_buf_start, _array, _array_start, _count) { \
    ByteArray __array = {.data = _array.data, .len = (_array).len}; \
    KineticLogger_ByteArraySliceToCString((char*)(_buf_start), (__array), (_array_start), (_count)); \
}

// #define LOG(message)  KineticLogger_Log(2, message)
#if !KINETIC_LOGGER_DISABLED

#define LOG0(message) KineticLogger_Log(0, message)
#define LOG1(message) KineticLogger_Log(1, message)
#define LOG2(message) KineticLogger_Log(2, message)
#define LOG3(message) KineticLogger_Log(3, message)
#define LOGF0(message, ...) KineticLogger_LogPrintf(0, message, __VA_ARGS__)
#define LOGF1(message, ...) KineticLogger_LogPrintf(1, message, __VA_ARGS__)
#define LOGF2(message, ...) KineticLogger_LogPrintf(2, message, __VA_ARGS__)
#define LOGF3(message, ...) KineticLogger_LogPrintf(3, message, __VA_ARGS__)
#define LOG_LOCATION  KineticLogger_LogLocation(__FILE__, __LINE__, __func__);
#define KINETIC_ASSERT(cond) { \
        if(!(cond)) \
        { \
            LOGF0("ASSERT FAILURE at %s:%d in %s: assert(" #cond ")", \
            __FILE__, (int)__LINE__, __FUNCTION__); \
            assert(cond); \
        } \
    }

#else

#define LOG0(message)
#define LOG1(message)
#define LOG2(message)
#define LOG3(message)
#define LOGF0(message, ...)
#define LOGF1(message, ...)
#define LOGF2(message, ...)
#define LOGF3(message, ...)
#define LOG_LOCATION
#define KINETIC_ASSERT(cond)

#endif

#endif // _KINETIC_LOGGER_H
