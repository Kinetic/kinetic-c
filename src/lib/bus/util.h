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
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

bool util_is_resumable_io_error(int errno_);

/* Get the current time. Returns false on failure, which should
 * never happen. If a time-adjustment-safe API is available on the
 * current OS (e.g. clock_gettime(CLOCK_MONOTONIC, ...), it will
 * be used when relative is true. If it's not available, the
 * relative flag has no impact. */
bool util_timestamp(struct timeval *tv, bool relative);

#endif
