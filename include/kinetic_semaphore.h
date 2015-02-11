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
#ifndef _KINETIC_SEMAPHORE_H
#define _KINETIC_SEMAPHORE_H

#include <stdbool.h>

typedef struct _KineticSemaphore KineticSemaphore;

/**
 * @brief Creates a KineticSemaphore. The KineticSemaphore is a simple wrapper 
 *        around a pthread condition variable and provides a a thread-safe
 *        way to block a thread and wait for notification from another thread.
 *
 * @return          Returns a pointer to a KineticSemaphore.
 */
KineticSemaphore * KineticSemaphore_Create(void);

/**
 * @brief Signals KineticSemaphore. This will unblock another 
 *        thread that's blocked on the given semaphore using KineticSemaphore_WaitForSignalAndDestroy()
 *        You should never signal the same KineticSemaphore more than once.
 *
 * @param sem       A pointer to the semaphore to signal.
 *
 */
void KineticSemaphore_Signal(KineticSemaphore * sem);

/**
 * @brief Reports whether the KineticSemaphore has been signaled.
 *
 * @param sem       A pointer to the semaphore to report signaled status from.
 *
 * @return          Returns true if signaled.
 */
bool KineticSemaphore_CheckSignaled(KineticSemaphore * sem);

/**
 * @brief Destorys the KineticSemaphore if it has been signaled.
 *
 * @param sem       A pointer to the semaphore to destroy.
 *
 * @return          Returns true signaled and detroyed.
 *                  Returns false if not yet signaled.
 */
bool KineticSemaphore_DestroyIfSignaled(KineticSemaphore * sem);

/**
 * @brief Blocks until the given semaphore is signaled. This will not block
 *        if the Semaphore has already been signaled. 
 *        Once unblocked, this will also destroy (free) the provide KineticSemaphore.
 *
 * @param sem       A pointer to the semaphore to wait for a signal.
 *
 */
void KineticSemaphore_WaitForSignalAndDestroy(KineticSemaphore * sem);

#endif // _KINETIC_SEMAPHORE_H

