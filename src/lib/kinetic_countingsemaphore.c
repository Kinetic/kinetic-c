#include "kinetic_countingsemaphore.h"
#include "kinetic_countingsemaphore_types.h"
#include <stdlib.h>
#include <assert.h>

KineticCountingSemaphore * KineticCountingSemaphore_Create(uint32_t counts)
{
    KineticCountingSemaphore * sem = calloc(1, sizeof(KineticCountingSemaphore));
    if (sem == NULL) { return NULL; }
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->available, NULL);
    sem->count = counts;
    return sem;
}

void KineticCountingSemaphore_Take(KineticCountingSemaphore * const sem)
{
    assert(sem != NULL);
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0)
    { pthread_cond_wait(&sem->available, &sem->mutex); }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
}

void KineticCountingSemaphore_Give(KineticCountingSemaphore * const sem)
{
    assert(sem != NULL);
    pthread_mutex_lock(&sem->mutex);
    if (sem->count == 0)
    { pthread_cond_signal(&sem->available); }
    sem->count++;
    pthread_mutex_unlock(&sem->mutex);
}

void KineticCountingSemaphore_Destroy(KineticCountingSemaphore * const sem)
{
    assert(sem != NULL);
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->available);
    free(sem);
}
