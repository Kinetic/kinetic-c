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
#include "kinetic_hmac.h"

#define PDU_HEADER_LEN      (1 + (2 * sizeof(int64_t)))
#define PDU_PROTO_MAX_LEN   (1024 * 1024)
#define PDU_VALUE_MAX_LEN   (1024 * 1024)
#define PDU_MAX_LEN         (PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)

#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary */
typedef struct _KineticPDUHeader
{
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;
#pragma pack(pop)   /* restore original alignment from stack */

typedef struct _KineticPDU
{
    // Binary PDU header (binary packed in NBO)
    KineticPDUHeader header;

    // Message associated with this PDU instance
    KineticMessage* protobuf;
    uint32_t protobufLength; // Embedded in header in NBO byte order (this is for reference)
    uint8_t protobufScratch[1024*1024];

    // Value data associated with PDU (if any)
    uint8_t* value;
    uint32_t valueLength; // Embedded in header in NBO byte order (this is for reference)

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticExchange* exchange;
} KineticPDU;

void KineticPDU_Init(
    KineticPDU* const pdu,
    KineticExchange* const exchange,
    KineticMessage* const message,
    uint8_t* const value,
    int32_t valueLength);

bool KineticPDU_Send(KineticPDU* const request);
bool KineticPDU_Receive(KineticPDU* const response);

#endif // _KINETIC_PDU_H
