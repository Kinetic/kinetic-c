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

#ifndef _KINETIC_SESSION_H
#define _KINETIC_SESSION_H

#include "kinetic_types_internal.h"

KineticStatus KineticSession_Create(KineticSession * const session, KineticClient * const client);
KineticStatus KineticSession_Destroy(KineticSession * const session);
KineticStatus KineticSession_Connect(KineticSession * const session);
KineticStatus KineticSession_Disconnect(KineticSession * const session);
KineticStatus KineticSession_GetTerminationStatus(KineticSession const * const session);
void KineticSession_SetTerminationStatus(KineticSession * const session, KineticStatus status);
int64_t KineticSession_GetNextSequenceCount(KineticSession * const session);
int64_t KineticSession_GetClusterVersion(KineticSession const * const session);
void KineticSession_SetClusterVersion(KineticSession * const session, int64_t cluster_version);
int64_t KineticSession_GetConnectionID(KineticSession const * const session);
void KineticSession_SetConnectionID(KineticSession * const session, int64_t id);

#endif // _KINETIC_SESSION_H
