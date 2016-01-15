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

#ifndef _KINETIC_COUNTINGSEMAPHORE_TYPES_H
#define _KINETIC_COUNTINGSEMAPHORE_TYPES_H

#include <pthread.h>
#include <stdint.h>

struct _KineticCountingSemaphore {
    pthread_mutex_t mutex;
    pthread_cond_t available;
    uint32_t count;
    uint32_t max;
    uint32_t num_waiting;
};

#endif // _KINETIC_COUNTINGSEMAPHORE_TYPES_H
