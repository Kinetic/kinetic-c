#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

void KineticResourceWaiter_Init(KineticResourceWaiter * const waiter)
{
    assert(waiter != NULL);
    pthread_mutex_init(&waiter->mutex, NULL);
    pthread_cond_init(&waiter->ready_cond, NULL);
}

void KineticResourceWaiter_SetAvailable(KineticResourceWaiter * const waiter)
{
    assert(waiter != NULL);
    pthread_mutex_lock(&waiter->mutex);

    waiter->ready = true;
    if (waiter->num_waiting > 0) {
        pthread_cond_signal(&waiter->ready_cond);
    }
    
    pthread_mutex_unlock(&waiter->mutex);
}

bool KineticResourceWaiter_WaitTilAvailable(KineticResourceWaiter * const waiter, uint32_t max_wait_sec)
{
    assert(waiter != NULL);
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
    assert(waiter != NULL);
    pthread_mutex_destroy(&waiter->mutex);
    pthread_cond_destroy(&waiter->ready_cond);
}
