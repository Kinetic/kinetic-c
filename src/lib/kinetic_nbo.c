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

#include "kinetic_nbo.h"

static uint64_t KineticNBO_SwapByteOrder(uint8_t* pByte, size_t len)
{
    uint64_t swapped = 0u;
    for (size_t i = 0; i < len; i++)
    {
        swapped = (swapped << 8) | pByte[i];
    }
    return swapped;
}

uint32_t KineticNBO_FromHostU32(uint32_t valueHost)
{
    return (uint32_t)KineticNBO_SwapByteOrder(
        (uint8_t*)&valueHost, sizeof(uint32_t));
}

uint32_t KineticNBO_ToHostU32(uint32_t valueNBO)
{
    return (uint32_t)KineticNBO_SwapByteOrder(
        (uint8_t*)&valueNBO, sizeof(uint32_t));
}

uint64_t KineticNBO_FromHostU64(uint64_t valueHost)
{
    return KineticNBO_SwapByteOrder(
        (uint8_t*)&valueHost, sizeof(uint64_t));
}

uint64_t KineticNBO_ToHostU64(uint64_t valueNBO)
{
    return KineticNBO_SwapByteOrder(
        (uint8_t*)&valueNBO, sizeof(uint64_t));
}
