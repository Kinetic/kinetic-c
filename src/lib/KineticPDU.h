/*
 * kinetic-c-client
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

#ifndef _KINETIC_PDU_H
#define _KINETIC_PDU_H

#include "KineticProto.h"

#define PDU_HEADER_LEN      (1 + (2 * sizeof(int64_t)))
#define PDU_PROTO_MAX_LEN   (1024 * 1024)
#define PDU_VALUE_MAX_LEN   (1024 * 1024)
#define PDU_MAX_LEN         (PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)

typedef struct _KineticPDU
{
    uint8_t* prefix;
    uint8_t* protoLength;
    uint8_t* valueLength;
    uint8_t* proto;
    uint8_t* value;
    uint8_t* data;
} KineticPDU;

void KineticPDU_Create(
    KineticPDU* const pdu,
    uint8_t* const buffer,
    const KineticProto* const proto,
    const uint8_t* const value,
    const int64_t valueLength);

#endif // _KINETIC_PDU_H
