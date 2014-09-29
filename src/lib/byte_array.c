#include "byte_array.h"
#include <assert.h>
#include <string.h>

ByteArray ByteArray_Create(void* data, size_t len)
{
    return (ByteArray) {
        .data = (uint8_t*)(data), .len = len
    };
}

ByteArray ByteArray_CreateWithCString(char* str)
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

ByteBuffer ByteBuffer_Create(void* data, size_t max_len)
{
    return (ByteBuffer) {
        .array = (ByteArray) {.data = (uint8_t*)data, .len = max_len},
        .bytesUsed = 0,
    };
}

ByteBuffer ByteBuffer_CreateWithArray(ByteArray array)
{
    return (ByteBuffer) {
        .array = array, .bytesUsed = 0
    };
}

long ByteBuffer_BytesRemaining(const ByteBuffer buffer)
{
    assert(buffer.array.data != NULL);
    return ((long)buffer.array.len - (long)buffer.bytesUsed);
}

ByteArray ByteBuffer_Consume(ByteBuffer* buffer, size_t len)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    if (buffer->bytesUsed + len > buffer->array.len) {
        return BYTE_ARRAY_NONE;
    }
    ByteArray slice = {
        .data = &buffer->array.data[buffer->bytesUsed],
        .len = len,
    };
    buffer->bytesUsed += len;
    return slice;
}

bool ByteBuffer_Append(ByteBuffer* buffer,
                       const void* data, size_t len)
{
    assert(buffer != NULL);
    assert(buffer->array.data != NULL);
    assert(data != NULL);
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
        return false;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], data, len);
    buffer->bytesUsed += len;
    return true;
}

bool ByteBuffer_AppendArray(ByteBuffer* buffer, const ByteArray array)
{
    assert(buffer->array.data != NULL);
    assert(array.data != NULL);
    if (array.len == 0 || ((buffer->bytesUsed + array.len) > buffer->array.len)) {
        return false;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], array.data, array.len);
    buffer->bytesUsed += array.len;
    return true;
}

bool ByteBuffer_AppendCString(ByteBuffer* buffer, const char* str)
{
    assert(buffer->array.data != NULL);
    assert(str != NULL);
    int len = strlen(str);
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
        return false;
    }
    memcpy(&buffer->array.data[buffer->bytesUsed], str, len);
    buffer->bytesUsed += len;
    return true;
}

#include <stdio.h>

bool ByteBuffer_AppendDummyData(ByteBuffer* buffer, size_t len)
{
    assert(buffer->array.data != NULL);
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buffer->array.data[buffer->bytesUsed + i] = (uint8_t)(i & 0x0FFu);
    }
    buffer->bytesUsed += len;
    return true;
}
