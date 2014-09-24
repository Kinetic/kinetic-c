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

#include "noop.h"

KineticPDU Request, Response;

int NoOp(
    const char* host,
    int port,
    bool nonBlocking,
    int64_t clusterVersion,
    int64_t identity,
    ByteArray hmacKey)
{
    KineticOperation operation;
    KineticConnection connection;
    bool success;

    KineticClient_Init(NULL);
    success = KineticClient_Connect(&connection, host, port, nonBlocking,
                                    clusterVersion, identity, hmacKey);
    assert(success);
    operation = KineticClient_CreateOperation(&connection, &Request, &Response);
    KineticStatus status = KineticClient_NoOp(&operation);
    KineticClient_Disconnect(&connection);

    if (status == KINETIC_STATUS_SUCCESS) {
        printf("NoOp operation completed successfully. Kinetic Device is alive and well!\n");
        return 0;
    }

    KineticClient_Disconnect(&connection);
    return (int)status;
}
