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

#ifndef _KINETIC_CONTROLLER_H
#define _KINETIC_CONTROLLER_H

#include "kinetic_types_internal.h"
#include "bus.h"

KineticStatus KineticController_Init(KineticSession * const session);
KineticStatus KineticController_ExecuteOperation(KineticOperation* operation, KineticCompletionClosure* closure);

void KineticController_HandleUnexpectedResponse(void *msg,
                                                int64_t seq_id,
                                                void *bus_udata,
                                                void *socket_udata);

void KineticController_HandleResult(bus_msg_result_t *res, void *udata);

#endif // _KINETIC_CONTROLLER_H
