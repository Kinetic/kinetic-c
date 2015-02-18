/*
* kinetic-c
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

#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
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
