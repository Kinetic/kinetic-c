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

#ifndef _KINETIC_ALLOCATOR_H
#define _KINETIC_ALLOCATOR_H

#include "kinetic_types_internal.h"

KineticSession* KineticAllocator_NewSession(struct bus * b, KineticSessionConfig* config);
void KineticAllocator_FreeSession(KineticSession* session);

KineticOperation* KineticAllocator_NewOperation(KineticSession* const session);
void KineticAllocator_FreeOperation(KineticOperation* operation);

KineticResponse * KineticAllocator_NewKineticResponse(size_t const valueLength);
void KineticAllocator_FreeKineticResponse(KineticResponse * response);

void KineticAllocator_FreeP2PProtobuf(Com__Seagate__Kinetic__Proto__Command__P2POperation* proto_p2pOp);

#endif // _KINETIC_ALLOCATOR
