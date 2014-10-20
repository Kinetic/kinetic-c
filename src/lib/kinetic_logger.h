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

#ifndef _KINETIC_LOGGER_H
#define _KINETIC_LOGGER_H

#include "kinetic_types_internal.h"
#include "kinetic_proto.h"
#include <stdarg.h>

#define KINETIC_LOG_FILE "kinetic.log"

void KineticLogger_Init(const char* logFile);
void KineticLogger_Close(void);
void KineticLogger_Log(int log_level, const char* message);
void KineticLogger_LogPrintf(int log_level, const char* format, ...);
void KineticLogger_LogLocation(const char* filename, int line, char const * message);
void KineticLogger_LogHeader(int log_level, const KineticPDUHeader* header);
void KineticLogger_LogProtobuf(int log_level, const KineticProto_Message* msg);
void KineticLogger_LogStatus(int log_level, KineticProto_Command_Status* status);
void KineticLogger_LogByteArray(int log_level, const char* title, ByteArray bytes);
void KineticLogger_LogByteBuffer(int log_level, const char* title, ByteBuffer buffer);

// #define LOG(message)  KineticLogger_Log(2, message)
#define LOG0(message) KineticLogger_Log(0, message)
#define LOG1(message) KineticLogger_Log(1, message)
#define LOG2(message) KineticLogger_Log(2, message)
#define LOG3(message) KineticLogger_Log(3, message)

// #define LOGF(message, ...)  KineticLogger_LogPrintf(2, message, __VA_ARGS__)
#define LOGF0(message, ...) KineticLogger_LogPrintf(0, message, __VA_ARGS__)
#define LOGF1(message, ...) KineticLogger_LogPrintf(1, message, __VA_ARGS__)
#define LOGF2(message, ...) KineticLogger_LogPrintf(2, message, __VA_ARGS__)
#define LOGF3(message, ...) KineticLogger_LogPrintf(3, message, __VA_ARGS__)

#define LOG_LOCATION  KineticLogger_LogLocation(__FILE__, __LINE__, __func__);

#endif // _KINETIC_LOGGER_H
