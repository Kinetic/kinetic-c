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

#include <stdbool.h>
#include <errno.h>
#include <time.h>

#include "util.h"

bool Util_IsResumableIOError(int errno_) {
    return errno_ == EAGAIN || errno_ == EINTR || errno_ == EWOULDBLOCK;
}

bool Util_Timestamp(struct timeval *tv, bool relative) {
    if (tv == NULL) {
        return false;
    }

#if 0
    if (relative) {
        struct timespec ts;
        if (0 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
            return false;
        }
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000L;
        return true;
    }
#else
    (void)relative;
#endif
    return (0 == gettimeofday(tv, NULL));
}
