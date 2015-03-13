/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#include "kinetic_response.h"
#include "kinetic_nbo.h"
#include "kinetic_session.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "kinetic_nbo.h"
#include "kinetic_allocator.h"
#include "kinetic_controller.h"
#include "kinetic_pdu_unpack.h"

#include <time.h>

uint32_t KineticResponse_GetProtobufLength(KineticResponse * response)
{
    KINETIC_ASSERT(response);
    return response->header.protobufLength;
}

uint32_t KineticResponse_GetValueLength(KineticResponse * response)
{
    KINETIC_ASSERT(response);
    return response->header.valueLength;
}

KineticStatus KineticResponse_GetStatus(KineticResponse * response)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (response != NULL &&
        response->command != NULL &&
        response->command->status != NULL &&
        response->command->status->has_code != false)
    {
        status = KineticProtoStatusCode_to_KineticStatus(
            response->command->status->code);
    }

    return status;
}

int64_t KineticResponse_GetConnectionID(KineticResponse * response)
{
    int64_t id = 0;
    KINETIC_ASSERT(response);
    KINETIC_ASSERT(response->proto);
    if (response->proto->authType == COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__UNSOLICITEDSTATUS &&
        response->command != NULL &&
        response->command->header != NULL &&
        response->command->header->has_connectionID)
    {
        id = response->command->header->connectionID;
    }
    return id;
}

Com__Seagate__Kinetic__Proto__Command__KeyValue* KineticResponse_GetKeyValue(KineticResponse * response)
{
    Com__Seagate__Kinetic__Proto__Command__KeyValue* keyValue = NULL;

    if (response != NULL &&
        response->command != NULL &&
        response->command->body != NULL)
    {
        keyValue = response->command->body->keyValue;
    }
    return keyValue;
}

Com__Seagate__Kinetic__Proto__Command__Range* KineticResponse_GetKeyRange(KineticResponse * response)
{
    Com__Seagate__Kinetic__Proto__Command__Range* range = NULL;
    if (response != NULL &&
        response->proto != NULL &&
        response->command != NULL &&
        response->command->body != NULL)
    {
        range = response->command->body->range;
    }
    return range;
}
