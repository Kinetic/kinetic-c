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
#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

void KineticResourceWaiter_Init(KineticResourceWaiter * const waiter)
{
    KINETIC_ASSERT(waiter != NULL);
    pthread_mutex_init(&waiter->mutex, NULL);
    pthread_cond_init(&waiter->ready_cond, NULL);
}

void KineticResourceWaiter_SetAvailable(KineticResourceWaiter * const waiter)
{
    KINETIC_ASSERT(waiter != NULL);
    pthread_mutex_lock(&waiter->mutex);

    waiter->ready = true;
    if (waiter->num_waiting > 0) {
        pthread_cond_signal(&waiter->ready_cond);
    }
    
    pthread_mutex_unlock(&waiter->mutex);
}

bool KineticResourceWaiter_WaitTilAvailable(KineticResourceWaiter * const waiter, uint32_t max_wait_sec)
{
    KINETIC_ASSERT(waiter != NULL);
    pthread_mutex_lock(&waiter->mutex);

    waiter->num_waiting++;

    struct timeval tv_current_time;
    gettimeofday(&tv_current_time, NULL);
    struct timespec ts_expire_time = {
        .tv_sec = tv_current_time.tv_sec + max_wait_sec,
        .tv_nsec = tv_current_time.tv_usec * 1000, // convert us to ns
    };

    int rc = 0;
    while (!waiter->ready && rc == 0) {
        rc = pthread_cond_timedwait(&waiter->ready_cond, &waiter->mutex, &ts_expire_time);
    }
    waiter->num_waiting--;

    pthread_mutex_unlock(&waiter->mutex);

    return (rc == 0);
}

void KineticResourceWaiter_Destroy(KineticResourceWaiter * const waiter)
{
    KINETIC_ASSERT(waiter != NULL);
    pthread_mutex_destroy(&waiter->mutex);
    pthread_cond_destroy(&waiter->ready_cond);
}
