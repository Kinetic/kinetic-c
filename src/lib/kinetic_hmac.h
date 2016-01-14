/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
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
