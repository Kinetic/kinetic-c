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
void KineticLogger_Log(const char* message);
int  KineticLogger_LogPrintf(const char* format, ...);
void KineticLogger_LogHeader(const KineticPDUHeader* header);
void KineticLogger_LogProtobuf(const KineticProto_Message* msg);
void KineticLogger_LogStatus(KineticProto_Command_Status* status);
void KineticLogger_LogByteArray(const char* title, ByteArray bytes);
void KineticLogger_LogByteBuffer(const char* title, ByteBuffer buffer);

#define LOG(message) KineticLogger_Log(message)
#define LOGF(message, ...) KineticLogger_LogPrintf(message, __VA_ARGS__)
#define LOG_LOCATION KineticLogger_LogPrintf("@ %s:%s:%d", __func__, __FILE__, __LINE__)

#endif // _KINETIC_LOGGER_H
