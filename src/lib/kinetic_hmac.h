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

#ifndef _KINETIC_HMAC_H
#define _KINETIC_HMAC_H

#include "kinetic_types_internal.h"
#include "kinetic.pb-c.h"

void KineticHMAC_Init(KineticHMAC* hmac,
                      Com__Seagate__Kinetic__Proto__Command__Security__ACL__HMACAlgorithm algorithm);

void KineticHMAC_Populate(KineticHMAC* hmac,
                          Com__Seagate__Kinetic__Proto__Message* msg,
                          const ByteArray key);

bool KineticHMAC_Validate(const Com__Seagate__Kinetic__Proto__Message* msg,
                          const ByteArray key);

#endif  // _KINETIC_HMAC_H
