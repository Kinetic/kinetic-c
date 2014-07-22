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

#ifndef _KINETIC_MESSAGE_H
#define _KINETIC_MESSAGE_H

#include "kinetic_proto.h"
#include <openssl/sha.h>

// #define KINETIC_HMAC_SHA1_LEN   (SHA_DIGEST_LENGTH)


typedef struct _KineticMessage
{
    // Kinetic Protocol Buffer Elements
    KineticProto            proto;
    KineticProto_Command    command;
    KineticProto_Header     header;
    KineticProto_Body       body;
    KineticProto_Status     status;
    uint8_t                 hmacData[SHA_DIGEST_LENGTH];
} KineticMessage;

void KineticMessage_Init(KineticMessage* const message);
void KineticMessage_BuildNoop(KineticMessage* const message);

#endif // _KINETIC_MESSAGE_H
