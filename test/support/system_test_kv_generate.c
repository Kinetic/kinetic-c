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
	return ByteBuffer_CreateAndAppendFormattedCString(key, MAX_SIZE, "%s%10d",KEY_PREFIX, index);
}

ByteBuffer generate_entry_value_by_index(uint32_t index)
{
	char* value = malloc(MAX_SIZE);
	return ByteBuffer_CreateAndAppendFormattedCString(value, MAX_SIZE, "%s%10d",VALUE_PREFIX, index);
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
