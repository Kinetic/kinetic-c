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

#ifndef BUS_POLL_H
#define BUS_POLL_H

#include <stdbool.h>
#include "bus_types.h"

/** Poll on fd until complete, return true on success or false on IO
 * error. (This is mostly in a distinct module to add a testing seam.) */
bool BusPoll_OnCompletion(struct bus *b, int fd);

#endif
