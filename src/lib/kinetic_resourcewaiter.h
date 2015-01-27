#ifndef _KINETIC_RESOURCEWAITER_H
#define _KINETIC_RESOURCEWAITER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct _KineticResourceWaiter KineticResourceWaiter;

void KineticResourceWaiter_Init(KineticResourceWaiter * const waiter);
void KineticResourceWaiter_SetAvailable(KineticResourceWaiter * const waiter);
bool KineticResourceWaiter_WaitTilAvailable(KineticResourceWaiter * const waiter, uint32_t max_wait_sec);
void KineticResourceWaiter_Destroy(KineticResourceWaiter * const waiter);

#endif // _KINETIC_RESOURCEWAITER_H
