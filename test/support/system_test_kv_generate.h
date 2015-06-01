/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#ifndef _SYSTEM_TEST_KV_GENERATE
#define _SYSTEM_TEST_KV_GENERATE

#include <stdint.h>
#include "byte_array.h"

ByteBuffer generate_entry_key_by_index(uint32_t index);
ByteBuffer generate_entry_value_by_index(uint32_t index);
ByteBuffer get_generated_value_by_key(ByteBuffer* key);

#endif  //_UNITY_KV_GENERATE
