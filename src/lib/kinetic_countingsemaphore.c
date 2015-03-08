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
#include "kinetic_logger.h"
#include <stdlib.h>
#include <assert.h>

KineticCountingSemaphore * KineticCountingSemaphore_Create(uint32_t counts)
{
    KineticCountingSemaphore * sem = calloc(1, sizeof(KineticCountingSemaphore));
    if (sem == NULL) { return NULL; }
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->available, NULL);
    sem->count = counts;
    sem->max = counts;
    sem->num_waiting = 0;
    return sem;
}

void KineticCountingSemaphore_Take(KineticCountingSemaphore * const sem) // WAIT
{
    assert(sem != NULL);
    pthread_mutex_lock(&sem->mutex);

    sem->num_waiting++;

    while (sem->count == 0) {
        pthread_cond_wait(&sem->available, &sem->mutex);
    }

    sem->num_waiting--;

    uint32_t before = sem->count--;
    uint32_t after = sem->count;
    uint32_t waiting = sem->num_waiting;
    
    pthread_mutex_unlock(&sem->mutex);
    
    LOGF3("Concurrent ops throttle -- TAKE: %u => %u (waiting=%u)", before, after, waiting);
}

void KineticCountingSemaphore_Give(KineticCountingSemaphore * const sem) // SIGNAL
{
    assert(sem != NULL);
    pthread_mutex_lock(&sem->mutex);
    
    if (sem->count == 0 && sem->num_waiting > 0) {
        pthread_cond_signal(&sem->available);
    }

    uint32_t before = sem->count++;
    uint32_t after = sem->count;
    uint32_t waiting = sem->num_waiting;
    
    pthread_mutex_unlock(&sem->mutex);
    
    LOGF3("Concurrent ops throttle -- GIVE: %u => %u (waiting=%u)", before, after, waiting);
    assert(sem->max >= after);
}

void KineticCountingSemaphore_Destroy(KineticCountingSemaphore * const sem)
{
    assert(sem != NULL);
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->available);
    free(sem);
}
