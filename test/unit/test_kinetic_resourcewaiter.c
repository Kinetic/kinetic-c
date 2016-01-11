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

#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "protobuf-c.h"
#include <stdlib.h>
#include <pthread.h>

KineticResourceWaiter ConnectionReady;

typedef struct {
    pthread_t threadID;
    KineticResourceWaiter* waiter;
    bool signaled;
} worker_args;

void* worker_thread(void* args)
{
    (void)args;
    return (void*)NULL;
}

#define MAX_COUNT 3
#define NUM_WORKERS 4
worker_args workers[NUM_WORKERS];

void test_kinetic_resourcewaiter_should_block_waiting_threads_until_available(void)
{
    KineticResourceWaiter_Init(&ConnectionReady);

    // Do stuff.....

    KineticResourceWaiter_SetAvailable(&ConnectionReady);

    KineticResourceWaiter_Destroy(&ConnectionReady);
}
