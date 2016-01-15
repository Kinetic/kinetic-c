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

#ifndef _KINETIC_RESOURCEWAITER_TYPES_H
#define _KINETIC_RESOURCEWAITER_TYPES_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

struct _KineticResourceWaiter {
    pthread_mutex_t mutex;
    pthread_cond_t ready_cond;
    bool ready;
    uint32_t num_waiting;
};

#endif // _KINETIC_RESOURCEWAITER_TYPES_H
