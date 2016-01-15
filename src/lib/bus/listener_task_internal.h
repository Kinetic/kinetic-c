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
