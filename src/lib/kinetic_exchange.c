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

#include "kinetic_exchange.h"
#include <string.h>
#include <time.h>

void KineticExchange_Init(
    KineticExchange* const exchange,
    int64_t identity,
    uint8_t* key,
    size_t keyLength,
    KineticConnection* const connection)
{
    KINETIC_EXCHANGE_INIT(exchange, identity, key, keyLength, connection);
}

void KineticExchange_ConfigureConnectionID(
    KineticExchange* const exchange)
{
    time_t cur_time;
    cur_time = time(NULL);
    exchange->connectionID = (int64_t)cur_time;
}

void KineticExchange_SetClusterVersion(
    KineticExchange* const exchange,
    int64_t clusterVersion)
{
    exchange->clusterVersion = clusterVersion;
    exchange->has_clusterVersion = true;
}

void KineticExchange_IncrementSequence(
    KineticExchange* const exchange)
{
    exchange->sequence++;
}

void KineticExchange_ConfigureHeader(
    const KineticExchange* const exchange,
    KineticProto_Header* const header)
{
    header->has_clusterversion = exchange->has_clusterVersion;
    if (exchange->has_clusterVersion)
    {
        header->clusterversion = exchange->clusterVersion;
    }
    header->has_identity = true;
    header->identity = exchange->identity;
    header->has_connectionid = true;
    header->connectionid = exchange->connectionID;
    header->has_sequence = true;
    header->sequence = exchange->sequence;
}
