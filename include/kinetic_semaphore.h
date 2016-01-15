/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
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

