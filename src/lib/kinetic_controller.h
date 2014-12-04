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

#ifndef _KINETIC_CONTROLLER_H
#define _KINETIC_CONTROLLER_H

#include "kinetic_types_internal.h"

KineticStatus KineticController_Init(KineticSession const * const session);
KineticOperation* KineticController_CreateOperation(KineticSession const * const session);
KineticStatus KineticController_ExecuteOperation(KineticOperation* operation, KineticCompletionClosure* closure);
void KineticController_Pause(KineticSession const * const session, bool pause);
void* KineticController_ReceiveThread(void* thread_arg);
void KineticController_HandleIncomingPDU(KineticConnection* const connection);

#endif // _KINETIC_CONTROLLER_H
