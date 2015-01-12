#include "kinetic_semaphore.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

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

