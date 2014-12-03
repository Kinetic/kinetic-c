#include "byte_array.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
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
    if (len == 0 || ((buffer->bytesUsed + len) > buffer->array.len)) {
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
    if (array.len == 0 || ((buffer->bytesUsed + array.len) > buffer->array.len)) {
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
