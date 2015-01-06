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

#ifndef _KINETIC_PDU_H
#define _KINETIC_PDU_H

#include "kinetic_types_internal.h"

bool KineticPDU_InitBus(int log_level, KineticClient * client);
void KineticPDU_DeinitBus(KineticClient * const client);
KineticStatus KineticPDU_GetStatus(KineticResponse* pdu);
KineticProto_Command_KeyValue* KineticPDU_GetKeyValue(KineticResponse* pdu);
KineticProto_Command_Range* KineticPDU_GetKeyRange(KineticResponse* pdu);

#endif // _KINETIC_PDU_H
