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

#ifndef _KINETIC_ENGINE_H
#define _KINETIC_ENGINE_H

#include "kinetic_types_internal.h"

typedef struct _KineticEngine {

} KineticEngine;

KineticStatus KineticEngine_Startup(KineticEngine * const engine);
KineticStatus KineticEngine_CreateConnection(KineticSession * const session);
KineticStatus KineticEngine_DestroyConnection(KineticSession * const session);
KineticStatus KineticEngine_EnqueueOperation(KineticSession * const session,
    KineticOperation * const operation, KineticOperationCallback const * const callback);
KineticStatus KineticEngine_Shutdown(KineticEngine * const engine);

#endif // _KINETIC_ENGINE_H
