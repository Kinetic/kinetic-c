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

#ifndef _KINETIC_HMAC_H
#define _KINETIC_HMAC_H

#include "kinetic_types.h"
#include "kinetic_message.h"

typedef struct _KineticHMAC
{
    KineticProto_Security_ACL_HMACAlgorithm algorithm;
    unsigned int valueLength;
    uint8_t value[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;

void KineticHMAC_Init(KineticHMAC * hmac, KineticProto_Security_ACL_HMACAlgorithm algorithm);
void KineticHMAC_Populate(KineticHMAC* hmac, KineticMessage* message, const char* const key, size_t keyLen);
bool KineticHMAC_Validate(const KineticProto* proto, const char* const key, size_t keyLen);

#endif  // _KINETIC_HMAC_H
