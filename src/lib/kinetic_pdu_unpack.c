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

#include "kinetic_pdu_unpack.h"

Com__Seagate__Kinetic__Proto__Command *KineticPDU_unpack_command(ProtobufCAllocator* allocator,
        size_t len, const uint8_t* data) {
    return com__seagate__kinetic__proto__command__unpack(allocator, len, data);
}

Com__Seagate__Kinetic__Proto__Message* KineticPDU_unpack_message(ProtobufCAllocator* allocator,
        size_t len, const uint8_t* data) {
    return com__seagate__kinetic__proto__message__unpack(allocator, len, data);
}
