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

#ifndef _BYTE_ARRAY_H
#define _BYTE_ARRAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Structure for handling generic arrays of bytes
 *
 * The data contained in a `ByteArray` is an arbitrary sequence of
 * bytes. It may contain embedded `NULL` characters and is not required to be
 * `NULL`-terminated.
 */
typedef struct _ByteArray {
    size_t  len;    /**< Number of bytes in the `data` field. */
    uint8_t* data;  /**< Pointer to an allocated array of data bytes. */
} ByteArray;

/** @brief Convenience macro to represent an empty array with no data */
#define BYTE_ARRAY_NONE (ByteArray){.len = 0, .data = NULL}

ByteArray ByteArray_Create(void* data, size_t len);
ByteArray ByteArray_CreateWithCString(const char* str);
ByteArray ByteArray_GetSlice(const ByteArray array, size_t start, size_t len);
void ByteArray_FillWithDummyData(const ByteArray array);

/**
 * @brief Structure for an embedded ByteArray as a buffer
 *
 * The `bytesUsed` field is initialized to zero, and is to incremented as each
 * byte is consumed, but shall not exceed the `array` length
 */
typedef struct {
    ByteArray array;  /**< ByteArray holding allocated array w/length = allocated size */
    size_t bytesUsed; /**< Reflects the number of bytes used from the `array` */
} ByteBuffer;

typedef struct {
    ByteBuffer* buffers;
    size_t count;
    size_t used;
} ByteBufferArray;

/** @brief Convenience macro to represent an empty buffer with no data */
#define BYTE_BUFFER_NONE (ByteBuffer){.array = BYTE_ARRAY_NONE, .bytesUsed = 0}

ByteBuffer ByteBuffer_Create(void* data, size_t max_len, size_t used);
ByteBuffer ByteBuffer_CreateWithArray(ByteArray array);
ByteBuffer ByteBuffer_CreateAndAppend(void* data, size_t max_len, const void* value, size_t value_len);
ByteBuffer ByteBuffer_CreateAndAppendArray(void* data, size_t max_len, const ByteArray value);
ByteBuffer ByteBuffer_CreateAndAppendCString(void* data, size_t max_len, const char* value);
ByteBuffer ByteBuffer_CreateAndAppendFormattedCString(void* data, size_t max_len, const char * format, ...);
ByteBuffer ByteBuffer_CreateAndAppendDummyData(void* data, size_t max_len, size_t len);
void ByteBuffer_Reset(ByteBuffer* buffer);
long ByteBuffer_BytesRemaining(const ByteBuffer buffer);
ByteArray ByteBuffer_Consume(ByteBuffer* buffer, size_t max_len);
ByteBuffer* ByteBuffer_Append(ByteBuffer* buffer, const void* data, size_t len);
ByteBuffer* ByteBuffer_AppendArray(ByteBuffer* buffer, const ByteArray array);
ByteBuffer* ByteBuffer_AppendBuffer(ByteBuffer* buffer, const ByteBuffer bufferToAppend);
ByteBuffer* ByteBuffer_AppendCString(ByteBuffer* buffer, const char* data);
ByteBuffer* ByteBuffer_AppendFormattedCString(ByteBuffer* buffer, const char * format, ...);
ByteBuffer* ByteBuffer_AppendDummyData(ByteBuffer* buffer, size_t len);
bool ByteBuffer_IsNull(ByteBuffer const buffer);
ByteBuffer ByteBuffer_Malloc(size_t size);
ByteBuffer ByteBuffer_MallocAndAppend(const void* data, size_t len);
void ByteBuffer_Free(ByteBuffer buffer);

#endif // _BYTE_ARRAY_H
