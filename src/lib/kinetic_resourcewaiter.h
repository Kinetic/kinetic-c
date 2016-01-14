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
