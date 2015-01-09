#ifndef _KINETIC_SEMAPHORE_H
#define _KINETIC_SEMAPHORE_H

struct _KineticSemaphore;
typedef struct _KineticSemaphore KineticSemaphore;

KineticSemaphore * KineticSemaphore_Create(void);
void KineticSemaphore_Lock(KineticSemaphore * sem);
void KineticSemaphore_Unlock(KineticSemaphore * sem);
void KineticSemaphore_Signal(KineticSemaphore * sem);
void KineticSemaphore_WaitForSignalAndDestroy(KineticSemaphore * sem);

#endif // _KINETIC_SEMAPHORE_H

