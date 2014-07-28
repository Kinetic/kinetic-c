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

#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "kinetic_types.h"
#include "kinetic_exchange.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"

void KineticApi_Init(
    const char* log_file);

bool KineticApi_Connect(
    KineticConnection* connection,
    const char* host,
    int port,
    bool blocking);

bool KineticApi_ConfigureExchange(
    KineticExchange* exchange,
    KineticConnection* connection,
    int64_t identity,
    uint8_t* key,
    size_t keyLength);

KineticOperation KineticApi_CreateOperation(
    KineticExchange* exchange,
    KineticPDU* request,
    KineticMessage* requestMsg,
    KineticPDU* response,
    KineticMessage* responseMsg);

KineticProto_Status_StatusCode KineticApi_NoOp(
    KineticOperation* operation
    );

#endif // _KINETIC_API_H
