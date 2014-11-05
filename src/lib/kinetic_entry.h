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

#ifndef _KINETIC_ENTRY_H
#define _KINETIC_ENTRY_H

#include "kinetic_types_internal.h"

void KineticEntry_Init(KineticEntry* entry);
ByteBuffer* KineticEntry_GetVersion(KineticEntry* entry);
void KineticEntry_SetVersion(KineticEntry* entry, ByteBuffer version);
ByteBuffer* KineticEntry_GetTag(KineticEntry* entry);
void KineticEntry_SetTag(KineticEntry* entry, ByteBuffer tag);
KineticAlgorithm KineticEntry_GetAlgorithm(KineticEntry* entry);
void KineticEntry_SetAlgorithm(KineticEntry* entry, KineticAlgorithm algorithm);

#endif // _KINETIC_ENTRY_H
