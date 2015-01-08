#include "kinetic_semaphore.h"
#include <pthread.h>
#include <stdlib.h>

struct _KineticSemaphore
{
    pthread_mutex_t mutex;
    pthread_cond_t complete;
};

KineticSemaphore * KineticSemaphore_Create(void)
{
    KineticSemaphore * sem = calloc(1, sizeof(KineticSemaphore));
    if (sem != NULL)
    {
        pthread_mutex_init(&sem->mutex, NULL);
        pthread_cond_init(&sem->complete, NULL);
    }
    return sem;
}

void KineticSemaphore_Lock(KineticSemaphore * sem)
{
    pthread_mutex_lock(&sem->mutex);
}

void KineticSemaphore_Unlock(KineticSemaphore * sem)
{
    pthread_mutex_unlock(&sem->mutex); 
}

void KineticSemaphore_Signal(KineticSemaphore * sem)
{
    pthread_cond_signal(&sem->complete);
}

void KineticSemaphore_WaitForSignalAndDestroy(KineticSemaphore * sem)
{
    pthread_mutex_lock(&sem->mutex);
    pthread_cond_wait(&sem->complete, &sem->mutex);
    pthread_mutex_unlock(&sem->mutex); 

    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->complete);

    free(sem);
}

