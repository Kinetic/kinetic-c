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
#include "system_test_kv_generate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

static const int MAX_SIZE = 1024;
static const char KEY_PREFIX[] = "my_key";
static const char VALUE_PREFIX[] = "my_value";

ByteBuffer generate_entry_key_by_index(uint32_t index)
{
	char* key = malloc(MAX_SIZE);
	return ByteBuffer_CreateAndAppendFormattedCString(key, MAX_SIZE, "%s%d",KEY_PREFIX, index);
}

ByteBuffer generate_entry_value_by_index(uint32_t index)
{
	char* value = malloc(MAX_SIZE);
	return ByteBuffer_CreateAndAppendFormattedCString(value, MAX_SIZE, "%s%d",VALUE_PREFIX, index);
}

ByteBuffer get_generated_value_by_key(ByteBuffer* key)
{
	assert(key != NULL);
	assert(key->array.data != NULL);
	assert(key->bytesUsed > 0);
	assert(key->array.len > 0);
	//assert(strncmp(key->array.data, KEY_PREFIX, strlen(KEY_PREFIX)) == 0);

	ByteBuffer value_buffer;
	char* value = malloc(MAX_SIZE);
	sprintf(value, "%s%s", VALUE_PREFIX, key->array.data + strlen(KEY_PREFIX));

	value_buffer.array.data = (uint8_t *)value;
	value_buffer.array.len = strlen(value);
	value_buffer.bytesUsed = value_buffer.array.len;

    return value_buffer;
}
