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

#ifndef _KINETIC_EXCHANGE_H
#define _KINETIC_EXCHANGE_H

#include "kinetic_types.h"
#include "kinetic_proto.h"
#include "kinetic_connection.h"

void KineticExchange_Init(
    KineticExchange* const exchange,
    int64_t identity,
    const char* key,
    size_t keyLength,
    KineticConnection* const connection);

void KineticExchange_ConfigureConnectionID(
    KineticExchange* const exchange);

void KineticExchange_SetClusterVersion(
    KineticExchange* const exchange,
    int64_t clusterVersion);

void KineticExchange_IncrementSequence(
    KineticExchange* const exchange);

void KineticExchange_ConfigureHeader(
    const KineticExchange* const exchange,
    KineticProto_Header* const header);

#endif // _KINETIC_EXCHANGE_H
