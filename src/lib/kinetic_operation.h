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

#ifndef _KINETIC_OPERATION_H
#define _KINETIC_OPERATION_H

#include "kinetic_types_internal.h"

KineticStatus KineticOperation_SendRequest(KineticOperation* const operation);
KineticOperation* KineticOperation_AssociateResponseWithOperation(KineticPDU* response);

struct timeval KineticOperation_GetTimeoutTime(KineticOperation* const operation);
void KineticOperation_SetTimeoutTime(KineticOperation* const operation, uint32_t const timeout_in_sec);
KineticStatus KineticOperation_GetStatus(const KineticOperation* const operation);

void KineticOperation_BuildNoop(KineticOperation* operation);
void KineticOperation_BuildPut(KineticOperation* const operation,
                               KineticEntry* const entry);
void KineticOperation_BuildGet(KineticOperation* const operation,
                               KineticEntry* const entry);
void KineticOperation_BuildDelete(KineticOperation* const operation,
                                  KineticEntry* const entry);

void KineticOperation_BuildGetKeyRange(KineticOperation* const operation,
                               KineticKeyRange* range, ByteBufferArray* buffers);

#endif // _KINETIC_OPERATION_H
