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

#include "kinetic_serial_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <sys/param.h>

KineticSerialAllocator KineticSerialAllocator_Create(size_t max_len) {
    return (KineticSerialAllocator) {
        .buffer = calloc(1, max_len),
        .total = max_len,
        .used = 0,
    };
}


void* KineticSerialAllocator_Base(KineticSerialAllocator* allocator)
{
    assert(allocator != NULL);
    return allocator->buffer;
}

void* KineticSerialAllocator_AllocateChunk(KineticSerialAllocator* allocator, size_t len)
{
    void* data = NULL;
    
    if (allocator->buffer != NULL ||
        ((unsigned long long)allocator->buffer) % sizeof(uint64_t) == 0 || // Ensure root of buffer is long-aligned
        allocator->used < allocator->total)
    {
        // Align to the next allocated hunk of data to the next 64-bit boundary
        size_t newUsed = allocator->used + len;

        // Assign pointer to new hunk of data
        if (newUsed <= allocator->total) {
            data = (void*)&allocator->buffer[allocator->used];
        }

        // Mark buffer full, if the current allocation has exhausted allocated data
        size_t padding = (sizeof(uint64_t) - 1);
        newUsed += padding;
        newUsed &= ~(sizeof(uint64_t) - 1);
        allocator->used = MIN(newUsed, allocator->total);
    }

    return data;
}
