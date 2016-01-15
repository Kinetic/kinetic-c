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

#include "kinetic_countingsemaphore.h"
#include "kinetic_countingsemaphore_types.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "protobuf-c.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    pthread_t threadID;
    KineticCountingSemaphore* sem;
} worker_args;

void* worker_thread(void* args)
{
    (void)args;
    return (void*)NULL;
}

#define MAX_COUNT 3
#define NUM_WORKERS 4
worker_args workers[NUM_WORKERS];

void test_kinetic_countingsemaphore_should_be_thread_safe(void)
{
    KineticCountingSemaphore* sem = KineticCountingSemaphore_Create(3);

    KineticCountingSemaphore_Take(sem);
    KineticCountingSemaphore_Take(sem);
    KineticCountingSemaphore_Take(sem);
    KineticCountingSemaphore_Give(sem);
    KineticCountingSemaphore_Give(sem);
    KineticCountingSemaphore_Take(sem);
    KineticCountingSemaphore_Give(sem);
    KineticCountingSemaphore_Give(sem);

    KineticCountingSemaphore_Destroy(sem);
}
