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

#include "kinetic_entry.h"
#include "kinetic_logger.h"

void KineticEntry_Init(KineticEntry* entry)
{
    KINETIC_ASSERT(entry != NULL);
    *entry = (KineticEntry) {
        .key = BYTE_BUFFER_NONE,
        .value = BYTE_BUFFER_NONE
    };
}

ByteBuffer* KineticEntry_GetVersion(KineticEntry* entry)
{
    KINETIC_ASSERT(entry != NULL);
    return &entry->dbVersion;
}

void KineticEntry_SetVersion(KineticEntry* entry, ByteBuffer version)
{
    KINETIC_ASSERT(entry != NULL);
    entry->dbVersion = version;
}

ByteBuffer* KineticEntry_GetTag(KineticEntry* entry)
{
    KINETIC_ASSERT(entry != NULL);
    return &entry->tag;
}

void KineticEntry_SetTag(KineticEntry* entry, ByteBuffer tag)
{
    KINETIC_ASSERT(entry != NULL);
    entry->tag = tag;
}

KineticAlgorithm KineticEntry_GetAlgorithm(KineticEntry* entry)
{
    KINETIC_ASSERT(entry != NULL);
    return entry->algorithm;
}

void KineticEntry_SetAlgorithm(KineticEntry* entry, KineticAlgorithm algorithm)
{
    KINETIC_ASSERT(entry != NULL);
    entry->algorithm = algorithm;
}
