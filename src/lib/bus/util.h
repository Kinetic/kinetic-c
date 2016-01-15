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

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

bool Util_IsResumableIOError(int errno_);

/* Get the current time. Returns false on failure, which should
 * never happen. If a time-adjustment-safe API is available on the
 * current OS (e.g. clock_gettime(CLOCK_MONOTONIC, ...), it will
 * be used when relative is true. If it's not available, the
 * relative flag has no impact. */
bool Util_Timestamp(struct timeval *tv, bool relative);

#endif
