#ifndef _KINETIC_RESOURCEWAITER_TYPES_H
#define _KINETIC_RESOURCEWAITER_TYPES_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

struct _KineticResourceWaiter {
    pthread_mutex_t mutex;
    pthread_cond_t ready_cond;
    bool ready;
    uint32_t num_waiting;
};

#endif // _KINETIC_RESOURCEWAITER_TYPES_H
