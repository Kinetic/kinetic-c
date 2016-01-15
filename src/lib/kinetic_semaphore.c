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

#include "kinetic_semaphore.h"
#include <pthread.h>
#include <stdlib.h>

struct _KineticSemaphore
{
    pthread_mutex_t mutex;
    pthread_cond_t complete;
    bool signaled;
};

KineticSemaphore * KineticSemaphore_Create(void)
{
    KineticSemaphore * sem = calloc(1, sizeof(KineticSemaphore));
    if (sem != NULL)
    {
        pthread_mutex_init(&sem->mutex, NULL);
        pthread_cond_init(&sem->complete, NULL);
        sem->signaled = false;
    }
    return sem;
}

void KineticSemaphore_Signal(KineticSemaphore * sem)
{
    pthread_mutex_lock(&sem->mutex);
    sem->signaled = true;
    pthread_cond_signal(&sem->complete);
    pthread_mutex_unlock(&sem->mutex);
}

bool KineticSemaphore_CheckSignaled(KineticSemaphore * sem)
{
    return sem->signaled;
}

bool KineticSemaphore_DestroyIfSignaled(KineticSemaphore * sem)
{
    if (sem->signaled) {
        pthread_mutex_destroy(&sem->mutex);
        pthread_cond_destroy(&sem->complete);
        free(sem);
        return true;
    }
    else {
        return false; // Semaphore has not yet been signaled
    }
}

void KineticSemaphore_WaitForSignalAndDestroy(KineticSemaphore * sem)
{
    pthread_mutex_lock(&sem->mutex);
    if (!sem->signaled) {
        pthread_cond_wait(&sem->complete, &sem->mutex);
    }
    pthread_mutex_unlock(&sem->mutex);

    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->complete);

    free(sem);
}

