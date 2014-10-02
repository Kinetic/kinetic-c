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
    const size_t  len;    /**< Number of bytes in the `data` field. */
    uint8_t* const data;  /**< Pointer to an allocated array of data bytes. */
} ByteArray;

/** @brief Convenience macro to represent an empty array with no data */
#define BYTE_ARRAY_NONE (ByteArray){.len = 0, .data = NULL}

ByteArray ByteArray_Create(void* data, size_t len);
ByteArray ByteArray_CreateWithCString(char* str);
void ByteArray_FillWithDummyData(const ByteArray array);
ByteArray ByteArray_GetSlice(const ByteArray array, size_t start, size_t len);

/**
 * @brief Structure for an embedded ByteArray as a buffer
 *
 * The `bytesUsed` field is initialized to zero, and is to incremented as each
 * byte is consumed, but shall not exceed the `array` length
 */
typedef struct {
    const ByteArray array;     /**< ByteArray holding allocated array w/length = allocated size */
    size_t          bytesUsed; /**< Reflects the number of bytes used from the `array` */
} ByteBuffer;

/** @brief Convenience macro to represent an empty buffer with no data */
#define BYTE_BUFFER_NONE (ByteBuffer){.array = BYTE_ARRAY_NONE}

ByteBuffer ByteBuffer_Create(void* data, size_t max_len);
ByteBuffer ByteBuffer_CreateWithArray(ByteArray array);
void ByteBuffer_Reset(ByteBuffer* buffer);
long ByteBuffer_BytesRemaining(const ByteBuffer buffer);
ByteArray ByteBuffer_Consume(ByteBuffer* buffer, size_t len);
ByteBuffer* ByteBuffer_Append(ByteBuffer* buffer, const void* data, size_t len);
ByteBuffer* ByteBuffer_AppendArray(ByteBuffer* buffer, const ByteArray array);
ByteBuffer* ByteBuffer_AppendCString(ByteBuffer* buffer, const char* data);
ByteBuffer* ByteBuffer_AppendDummyData(ByteBuffer* buffer, size_t len);

#endif // _BYTE_ARRAY_H
