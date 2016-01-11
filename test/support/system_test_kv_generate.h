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

#ifndef _SYSTEM_TEST_KV_GENERATE
#define _SYSTEM_TEST_KV_GENERATE

#include <stdint.h>
#include "byte_array.h"

ByteBuffer generate_entry_key_by_index(uint32_t index);
ByteBuffer generate_entry_value_by_index(uint32_t index);
ByteBuffer get_generated_value_by_key(ByteBuffer* key);

#endif  //_UNITY_KV_GENERATE
