/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#include "kinetic_countingsemaphore.h"
#include "kinetic_countingsemaphore_types.h"
#include "unity.h"
#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
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

    // Do stuff.....

    KineticCountingSemaphore_Destroy(sem);
}
