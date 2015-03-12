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
#ifndef LISTENER_TASK_INTERNAL_H
#define LISTENER_TASK_INTERNAL_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener_internal_types.h"

/** Coefficients for backpressure based on certain conditions. */
#define MSG_BP_1QTR       (0.25)
#define MSG_BP_HALF       (0.5)
#define MSG_BP_3QTR       (2.0)
#define RX_INFO_BP_1QTR   (0.5)
#define RX_INFO_BP_HALF   (0.5)
#define RX_INFO_BP_3QTR   (2.0)
#define THREADPOOL_BP     (1.0)

#endif
