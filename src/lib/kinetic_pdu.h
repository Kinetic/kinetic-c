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

#ifndef _KINETIC_PDU_H
#define _KINETIC_PDU_H

#include "kinetic_types.h"
#include "kinetic_exchange.h"
#include "kinetic_message.h"

#define PDU_HEADER_LEN      (1 + (2 * sizeof(int64_t)))
#define PDU_PROTO_MAX_LEN   (1024 * 1024)
#define PDU_VALUE_MAX_LEN   (1024 * 1024)
#define PDU_MAX_LEN         (PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)

typedef struct _KineticPDU
{
    // PDU Fields
    uint8_t* prefix;
    uint8_t* protoLength;
    uint8_t* valueLength;
    uint8_t* proto;
    uint8_t* value;
    uint8_t* data;

    // Exchange associated with this PDU instance
    KineticExchange* exchange;

    // Message associated with this PDU instance
    KineticMessage* message;
} KineticPDU;

void KineticPDU_Init(
    KineticPDU* const pdu,
    KineticExchange* const exchange,
    uint8_t* const buffer,
    KineticMessage* const message,
    const uint8_t* const value,
    int64_t valueLength);

#endif // _KINETIC_PDU_H
