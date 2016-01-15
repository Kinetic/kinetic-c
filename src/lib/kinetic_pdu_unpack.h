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

#ifndef KINETIC_PDU_UNPACK_H
#define KINETIC_PDU_UNPACK_H

#include "kinetic.pb-c.h"

/* This wrapper only exists for mocking purposes. */
Com__Seagate__Kinetic__Proto__Command *KineticPDU_unpack_command(ProtobufCAllocator* allocator,
    size_t len, const uint8_t* data);

Com__Seagate__Kinetic__Proto__Message* KineticPDU_unpack_message(ProtobufCAllocator* allocator,
    size_t len, const uint8_t* data);

#endif
