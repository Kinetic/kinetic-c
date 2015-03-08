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

#include "kinetic_entry.h"

// KineticEntry - byte arrays need to be preallocated by the client
// typedef struct _KineticEntry {
//     ByteBuffer key;
//     ByteBuffer value;

//     // Metadata
//     ByteBuffer dbVersion;
//     ByteBuffer tag;
//     KineticAlgorithm algorithm;

//     // Operation-specific attributes
//     // (TODO: remove from struct, and specify a attributes to PUT/GET operations)
//     ByteBuffer newVersion;
//     bool metadataOnly;
//     bool force;
//     KineticSynchronization synchronization;
// } KineticEntry;

void KineticEntry_Init(KineticEntry* entry)
{
    assert(entry != NULL);
    *entry = (KineticEntry) {
        .key = BYTE_BUFFER_NONE,
        .value = BYTE_BUFFER_NONE
    };
}

ByteBuffer* KineticEntry_GetVersion(KineticEntry* entry)
{
    assert(entry != NULL);
    return &entry->dbVersion;
}

void KineticEntry_SetVersion(KineticEntry* entry, ByteBuffer version)
{
    assert(entry != NULL);
    entry->dbVersion = version;
}

ByteBuffer* KineticEntry_GetTag(KineticEntry* entry)
{
    assert(entry != NULL);
    return &entry->tag;
}

void KineticEntry_SetTag(KineticEntry* entry, ByteBuffer tag)
{
    assert(entry != NULL);
    entry->tag = tag;
}

KineticAlgorithm KineticEntry_GetAlgorithm(KineticEntry* entry)
{
    assert(entry != NULL);
    return entry->algorithm;
}

void KineticEntry_SetAlgorithm(KineticEntry* entry, KineticAlgorithm algorithm)
{
    assert(entry != NULL);
    entry->algorithm = algorithm;
}
