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
