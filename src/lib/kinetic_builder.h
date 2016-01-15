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

#ifndef _KINETIC_BUILDER_H
#define _KINETIC_BUILDER_H

#include "kinetic_types_internal.h"

/* Operations with non-standard timeouts. */
typedef enum {
    KineticOperation_TimeoutSetACL = 30,
	KineticOperation_TimeoutMediaScan = 30,
	KineticOperation_TimeoutMediaOptimize = 30,
    KineticOperation_TimeoutSetPin = 30,
    KineticOperation_TimeoutLockUnlock = 30,
    KineticOperation_TimeoutErase = 180,
} KineticOperation_NonstandardTimeout;

/*******************************************************************************
 * Standard Client Operations
*******************************************************************************/
KineticStatus KineticBuilder_BuildNoop(KineticOperation* op);
KineticStatus KineticBuilder_BuildPut(KineticOperation* const op,
    KineticEntry* const entry);
KineticStatus KineticBuilder_BuildGet(KineticOperation* const op,
    KineticEntry* const entry);
KineticStatus KineticBuilder_BuildGetNext(KineticOperation* const op,
    KineticEntry* const entry);
KineticStatus KineticBuilder_BuildGetPrevious(KineticOperation* const op,
    KineticEntry* const entry);
KineticStatus KineticBuilder_BuildFlush(KineticOperation* const op);
KineticStatus KineticBuilder_BuildDelete(KineticOperation* const op,
    KineticEntry* const entry);
KineticStatus KineticBuilder_BuildGetKeyRange(KineticOperation* const op,
    KineticKeyRange* range, ByteBufferArray* buffers);
KineticStatus KineticBuilder_BuildP2POperation(KineticOperation* const op,
    KineticP2P_Operation* const p2pOp);

/*******************************************************************************
 * Admin Client Operations
*******************************************************************************/
KineticStatus KineticBuilder_BuildGetLog(KineticOperation* const op,
    Com__Seagate__Kinetic__Proto__Command__GetLog__Type type, ByteArray name, KineticLogInfo** info);
KineticStatus KineticBuilder_BuildSetPin(KineticOperation* const op,
    ByteArray old_pin, ByteArray new_pin, bool lock);
KineticStatus KineticBuilder_BuildErase(KineticOperation* const op,
    bool secure_erase, ByteArray* pin);
KineticStatus KineticBuilder_BuildLockUnlock(KineticOperation* const op,
    bool lock, ByteArray* pin);
KineticStatus KineticBuilder_BuildSetACL(KineticOperation* const op,
    struct ACL *ACLs);
KineticStatus KineticBuilder_BuildSetClusterVersion(KineticOperation* const op,
    int64_t new_cluster_version);
KineticStatus KineticBuilder_BuildMediaScan(KineticOperation* const op,
	const KineticMediaScan_Operation* mediascan_operation, KineticCommand_Priority priority);
KineticStatus KineticBuilder_BuildMediaOptimize(KineticOperation* const op,
	const KineticMediaOptimize_Operation* mediaoptimize_operation, KineticCommand_Priority priority);
KineticStatus KineticBuilder_BuildUpdateFirmware(KineticOperation* const op,
    const char* fw_path);

#endif // _KINETIC_BUILDER_H
