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
#include <stdbool.h>
#include <errno.h>
#include <time.h>

#include "util.h"

bool util_is_resumable_io_error(int errno_) {
    return errno_ == EAGAIN || errno_ == EINTR || errno_ == EWOULDBLOCK;
}

bool util_timestamp(struct timeval *tv, bool relative) {
    if (tv == NULL) {
        return false;
    }

#ifdef __linux__
    if (relative) {
        struct timespec ts;
        if (0 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
            return false;
        }
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = tv.tv_nsec / 1000L;
        return true;
    }
#else
    (void)relative;
#endif
    return (0 == gettimeofday(tv, NULL));
}
