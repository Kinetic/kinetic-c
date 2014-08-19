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

#ifndef _KINETIC_PDU_H
#define _KINETIC_PDU_H

#include "kinetic_types.h"

void KineticPDU_Init(KineticPDU* const pdu,
    KineticConnection* const connection,
    KineticMessage* const message,
    uint8_t* const value,
    int32_t valueLength);
KineticProto_Status_StatusCode KineticPDU_Status(KineticPDU* const pdu);

bool KineticPDU_Send(KineticPDU* const request);
bool KineticPDU_Receive(KineticPDU* const response);

#endif // _KINETIC_PDU_H
