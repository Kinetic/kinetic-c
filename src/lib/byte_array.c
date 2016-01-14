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

#include "byte_array.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/param.h>

static ByteBuffer* append_formatted_cstring_va_list(ByteBuffer* buffer,
    const char* format, va_list args);

ByteArray ByteArray_Create(void* data, size_t len)
{
    return (ByteArray) {
        .data = (uint8_t*)(data), .len = len
    };
}

ByteArray ByteArray_CreateWithCString(const char* str)
{
    return (ByteArray) {
        .data = (uint8_t*)str, .len = strlen(str)
    };
}

void ByteArray_FillWithDummyData(ByteArray array)
{
    for (size_t i = 0; i < array.len; i++) {
        array.data[i] = (uint8_t)(i & 0x0FFu);
    }
}

ByteArray ByteArray_GetSlice(ByteArray array, size_t start, size_t len)
{
    assert(array.data != NULL);
    assert(start < array.len);
    assert(start + len <= array.len);
    return (ByteArray) {
        .data = &array.data[start], .len = len
    };
}

void ByteBuffer_Reset(ByteBuffer* buffer)
{
    assert(buffer != NULL);
    buffer->bytesUsed = 0;
}

ByteBuffer ByteBuffer_Create(void* data, size_t max_len, size_t used)
{
    assert(data != NULL);
    assert(max_len > 0);
    return (ByteBuffer) {
        .array = (ByteArray) {.data = (uint8_t*)data, .len = max_len},
        .bytesUsed = used,
    };
}

ByteBuffer ByteBuffer_CreateWithArray(ByteArray array)
{
    return (ByteBuffer) {.array = array, .bytesUsed = 0};
}

ByteBuffer ByteBuffer_CreateAndAppend(void* data, size_t max_len, const void* value, size_t value_len)
{
    ByteBuffer buf = ByteBuffer_Create(data, max_len, 0);
    ByteBuffer_Append(&buf, value, value_len);
    return buf;
}

ByteBuffer ByteBuffer_CreateAndAppendArray(void* data, size_t max_len, const ByteArray value)
{
    ByteBuffer buf = ByteBuffer_Create(data, max_len, 0);
    ByteBuffer_AppendArray(&buf, value);
    return buf;
}

ByteBuffer ByteBuffer_CreateAndAppendCString(void* data, size_t max_len, const char* value)
{
    ByteBuffer buf = ByteBuffer_Create(data, max_len, 0);
    ByteBuffer_AppendCString(&buf, value);
    return buf;
}

ByteBuffer ByteBuffer_CreateAndAppendDummyData(void* data, size_t max_len, size_t len)
{
    ByteBuffer buf = ByteBuffer_Create(data, max_len, 0);
    ByteBuffer_AppendDummyData(&buf, len);
    return buf;
}

long ByteBuffer_BytesRemaining(const ByteBuffer buffer)
{
    assert(buffer.array.data != NULL);
    return ((long)buffer.array.len - (long)buffer.bytesUsed);
}

ByteArray ByteBuffer_Consume(ByteBuffer* buffer, size_t max_len)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    if (buffer->bytesUsed >= buffer->array.len) {
        return BYTE_ARRAY_NONE;
    }
    long remaining = ByteBuffer_BytesRemaining(*buffer);
    assert(remaining >= 0);
    size_t len = MIN(max_len, (size_t)remaining);
    ByteArray slice = {
        .data = &buffer->array.data[buffer->bytesUsed],
        .len = len,
    };
    buffer->bytesUsed += len;
    return slice;
}

ByteBuffer* ByteBuffer_Append(ByteBuffer* buffer, const void* data, size_t len)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    assert(data != NULL);
    if ((buffer->bytesUsed + len) > buffer->array.len) {
        return NULL;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], data, len);
    buffer->bytesUsed += len;
    assert(buffer != NULL);
    return buffer;
}

ByteBuffer* ByteBuffer_AppendArray(ByteBuffer* buffer, const ByteArray array)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    assert(array.data != NULL);
    if ((buffer->bytesUsed + array.len) > buffer->array.len) {
        return NULL;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], array.data, array.len);
    buffer->bytesUsed += array.len;
    return buffer;
}

ByteBuffer* ByteBuffer_AppendBuffer(ByteBuffer* buffer, const ByteBuffer bufferToAppend)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    assert(bufferToAppend.array.data != NULL);
    assert(bufferToAppend.bytesUsed <= bufferToAppend.array.len);
    if ((buffer->bytesUsed + bufferToAppend.bytesUsed) > buffer->array.len) {
        return NULL;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], bufferToAppend.array.data, bufferToAppend.bytesUsed);
    buffer->bytesUsed += bufferToAppend.bytesUsed;
    return buffer;
}

ByteBuffer* ByteBuffer_AppendCString(ByteBuffer* buffer, const char* str)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    assert(str != NULL);
    int len = strlen(str);
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
        return NULL;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], str, len);
    buffer->bytesUsed += len;
    return buffer;
}


static ByteBuffer* append_formatted_cstring_va_list(ByteBuffer* buffer, const char* format, va_list args)
{
    char* start = (char*)&buffer->array.data[buffer->bytesUsed];
    long startLen = (long)buffer->bytesUsed;
    long maxLen = buffer->array.len;
    long remainingLen = ByteBuffer_BytesRemaining(*buffer);
    long extraLen = vsnprintf(start, remainingLen, format, args);
    if (startLen + extraLen >= maxLen) {
        return NULL;
    }
    buffer->bytesUsed += extraLen;
    return buffer;
}

ByteBuffer* ByteBuffer_AppendFormattedCString(ByteBuffer* buffer, const char * format, ...)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);

    bool failed = false;
    va_list args;
    va_start(args, format);

    failed = (append_formatted_cstring_va_list(buffer, format, args) == NULL);

    va_end(args);

    return failed ? NULL : buffer;
}

ByteBuffer ByteBuffer_CreateAndAppendFormattedCString(void* data, size_t max_len, const char * format, ...)
{
    ByteBuffer buf = ByteBuffer_Create(data, max_len, 0);

    va_list args;
    va_start(args, format);
    if (append_formatted_cstring_va_list(&buf, format, args) == NULL) {
        memset(&buf, 0, sizeof(ByteBuffer));
    }
    va_end(args);

    return buf;
}

ByteBuffer* ByteBuffer_AppendDummyData(ByteBuffer* buffer, size_t len)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        buffer->array.data[buffer->bytesUsed + i] = (uint8_t)(i & 0x0FFu);
    }
    buffer->bytesUsed += len;
    return buffer;
}

bool ByteBuffer_IsNull(ByteBuffer const buffer)
{
    return buffer.array.data == NULL;
}

ByteBuffer ByteBuffer_Malloc(size_t size)
{
    // There is not check on the return value from calloc by design
    //  the intention is that you can check for malloc failure by
    //  calling ByteBuffer_IsNull on the returned ByteBuffer
    return ByteBuffer_Create(calloc(1, size), size, 0);
}

ByteBuffer ByteBuffer_MallocAndAppend(const void* data, size_t len)
{
    assert(data != NULL);
    assert(len != 0);
    ByteBuffer buffer = ByteBuffer_Malloc(len);
    if (ByteBuffer_IsNull(buffer)) { return buffer; }
    ByteBuffer_Append(&buffer, data, len);
    return buffer;
}

void ByteBuffer_Free(ByteBuffer buffer)
{
    assert(buffer.array.data != NULL);
    free(buffer.array.data);
}
