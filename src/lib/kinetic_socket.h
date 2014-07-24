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

#ifndef _KINETIC_SOCKET_H
#define _KINETIC_SOCKET_H

#include "kinetic_types.h"
#include "kinetic_message.h"

int KineticSocket_Connect(const char* host, int port, bool blocking);
void KineticSocket_Close(int socketDescriptor);

bool KineticSocket_Read(int socketDescriptor, void* buffer, size_t length);
bool KineticSocket_ReadProtobuf(int socketDescriptor, KineticProto* message, void* buffer, size_t length);

bool KineticSocket_Write(int socketDescriptor, const void* buffer, size_t length);
bool KineticSocket_WriteProtobuf(int socketDescriptor, const KineticMessage* message);

#endif // _KINETIC_SOCKET_H
