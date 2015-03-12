/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#ifndef _KINETIC_RESPONSE_H
#define _KINETIC_RESPONSE_H

#include "kinetic_types_internal.h"

uint32_t KineticResponse_GetProtobufLength(KineticResponse * response);
uint32_t KineticResponse_GetValueLength(KineticResponse * response);
KineticStatus KineticResponse_GetStatus(KineticResponse * response);
int64_t KineticResponse_GetConnectionID(KineticResponse * response);
Com_Seagate_Kinetic_Proto_Command_KeyValue* KineticResponse_GetKeyValue(KineticResponse * response);
Com_Seagate_Kinetic_Proto_Command_Range* KineticResponse_GetKeyRange(KineticResponse * response);

#endif // _KINETIC_RESPONSE_H
