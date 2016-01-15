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

#include "kinetic_nbo.h"

static uint64_t KineticNBO_SwapByteOrder(uint8_t* pByte, size_t len)
{
    uint64_t swapped = 0u;
    for (size_t i = 0; i < len; i++) {
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
