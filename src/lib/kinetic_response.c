/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
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
    if (response->proto->authtype == COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__UNSOLICITEDSTATUS &&
        response->command != NULL &&
        response->command->header != NULL &&
        response->command->header->has_connectionid)
    {
        id = response->command->header->connectionid;
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
        keyValue = response->command->body->keyvalue;
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
