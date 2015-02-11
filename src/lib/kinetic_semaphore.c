/*
* kinetic-c-client
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

