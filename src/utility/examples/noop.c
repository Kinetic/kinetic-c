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

KineticStatus NoOp(KineticSession* session)
{
    KineticSessionHandle sessionHandle;

    KineticStatus status = KineticClient_Connect(session, &sessionHandle);
    if (status != KINETIC_STATUS_SUCCESS) {
        printf("Failed connecting to host %s:%d", session->host, session->port);
        return (int)status;
    }

    status = KineticClient_NoOp(sessionHandle);
    
    KineticClient_Disconnect(&sessionHandle);
    if (status == KINETIC_STATUS_SUCCESS) {
        printf("NoOp operation completed successfully. Kinetic Device is alive and well!\n");
    }
    else {
        printf("NoOp operations failed with status: %d", (int)status);
    }

    return status;
}
