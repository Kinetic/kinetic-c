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

#ifndef _KINETIC_CALLBACKS_H
#define _KINETIC_CALLBACKS_H

#include "kinetic_types_internal.h"

KineticStatus KineticCallbacks_Basic(KineticOperation* const operation, KineticStatus const status);

/*******************************************************************************
 * Standard Client Operations
*******************************************************************************/
KineticStatus KineticCallbacks_Put(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_Get(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_Delete(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_GetKeyRange(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_P2POperation(KineticOperation* const operation, KineticStatus const status);

/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/
KineticStatus KineticCallbacks_GetLog(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_SetClusterVersion(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_SetACL(KineticOperation* const operation, KineticStatus const status);
KineticStatus KineticCallbacks_UpdateFirmware(KineticOperation* const operation, KineticStatus const status);

#endif // _KINETIC_CALLBACKS_H
