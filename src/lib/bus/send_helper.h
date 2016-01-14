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

#ifndef SEND_HELPER_H
#define SEND_HELPER_H

#include "bus_types.h"
#include "bus_internal_types.h"

typedef enum {
    SHHW_OK,
    SHHW_DONE,
    SHHW_ERROR = -1,
} SendHelper_HandleWrite_res;

SendHelper_HandleWrite_res SendHelper_HandleWrite(bus *b, boxed_msg *box);

#endif
