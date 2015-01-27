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

#ifndef _KINETIC_ALLOCATOR_H
#define _KINETIC_ALLOCATOR_H

#include "kinetic_types_internal.h"

void KineticAllocator_InitLists(KineticConnection* connection);

KineticConnection* KineticAllocator_NewConnection(struct bus * b, KineticSession* const session);
void KineticAllocator_FreeConnection(KineticConnection* connection);

KineticPDU* KineticAllocator_NewPDU(KineticConnection* connection);
void KineticAllocator_FreePDU(KineticPDU* pdu);

KineticOperation* KineticAllocator_NewOperation(KineticConnection* connection);
void KineticAllocator_FreeOperation(KineticOperation* operation);

KineticResponse * KineticAllocator_NewKineticResponse(size_t const valueLength);
void KineticAllocator_FreeKineticResponse(KineticResponse * response);

#endif // _KINETIC_ALLOCATOR
