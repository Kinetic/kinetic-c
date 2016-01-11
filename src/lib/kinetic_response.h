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

#ifndef _KINETIC_RESPONSE_H
#define _KINETIC_RESPONSE_H

#include "kinetic_types_internal.h"

uint32_t KineticResponse_GetProtobufLength(KineticResponse * response);
uint32_t KineticResponse_GetValueLength(KineticResponse * response);
KineticStatus KineticResponse_GetStatus(KineticResponse * response);
int64_t KineticResponse_GetConnectionID(KineticResponse * response);
Com__Seagate__Kinetic__Proto__Command__KeyValue* KineticResponse_GetKeyValue(KineticResponse * response);
Com__Seagate__Kinetic__Proto__Command__Range* KineticResponse_GetKeyRange(KineticResponse * response);

#endif // _KINETIC_RESPONSE_H
