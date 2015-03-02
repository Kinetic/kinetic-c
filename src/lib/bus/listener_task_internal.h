/*
* kinetic-c-client
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
#ifndef LISTENER_TASK_INTERNAL_H
#define LISTENER_TASK_INTERNAL_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener_internal_types.h"

static void tick_handler(listener *l);
static void clean_up_completed_info(listener *l, rx_info_t *info);
static void retry_delivery(listener *l, rx_info_t *info);
static void observe_backpressure(listener *l, size_t backpressure);

#endif
