#ifndef _KINETIC_COUNTINGSEMAPHORE_H
#define _KINETIC_COUNTINGSEMAPHORE_H

#include <stdint.h>

typedef struct _KineticCountingSemaphore KineticCountingSemaphore;

KineticCountingSemaphore * KineticCountingSemaphore_Create(uint32_t max);
void KineticCountingSemaphore_Take(KineticCountingSemaphore * const sem);
void KineticCountingSemaphore_Give(KineticCountingSemaphore * const sem);
void KineticCountingSemaphore_Destroy(KineticCountingSemaphore * const sem);

#endif // _KINETIC_COUNTINGSEMAPHORE_H
