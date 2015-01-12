#ifndef _KINETIC_COUNTINGSEMAPHORE_TYPES_H
#define _KINETIC_COUNTINGSEMAPHORE_TYPES_H

#include <pthread.h>
#include <stdint.h>

struct _KineticCountingSemaphore {
    pthread_mutex_t mutex;
    pthread_cond_t available;
    uint32_t count;
};

#endif // _KINETIC_COUNTINGSEMAPHORE_TYPES_H
