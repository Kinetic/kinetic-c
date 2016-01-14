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
    KINETIC_ASSERT(sem != NULL);
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
    KINETIC_ASSERT(sem != NULL);
    pthread_mutex_lock(&sem->mutex);

    if (sem->count == 0 && sem->num_waiting > 0) {
        pthread_cond_signal(&sem->available);
    }

    uint32_t before = sem->count++;
    uint32_t after = sem->count;
    uint32_t waiting = sem->num_waiting;

    pthread_mutex_unlock(&sem->mutex);

    LOGF3("Concurrent ops throttle -- GIVE: %u => %u (waiting=%u)", before, after, waiting);
    KINETIC_ASSERT(sem->max >= after);
}

void KineticCountingSemaphore_Destroy(KineticCountingSemaphore * const sem)
{
    KINETIC_ASSERT(sem != NULL);
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->available);
    free(sem);
}
