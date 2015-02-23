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

KineticStatus KineticEngine_Startup(KineticEngine * const engine)
{
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticEngine_CreateConnection(KineticSession * const session)
{
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticEngine_DestroyConnection(KineticSession * const session)
{
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticEngine_EnqueueOperation(KineticSession * const session,
    KineticOperation * const operation, KineticOperationCallback const * const callback)
{
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticEngine_Shutdown(KineticEngine * const engine)
{
    return KINETIC_STATUS_INVALID;
}
