#include "unity.h"
#include "byte_array.h"
#include <string.h>
#include <stdio.h>

void test_BYTE_ARRAY_NONE_should_reflect_an_empty_array_with_non_allocated_data(void)
{
    ByteArray array = BYTE_ARRAY_NONE;

    TEST_ASSERT_EQUAL(0, array.len);
    TEST_ASSERT_NULL(array.data);
}

void test_ByteArray_Create_should_create_a_byte_array(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u};
    size_t len = sizeof(data);

    ByteArray array = ByteArray_Create(data, len);

    TEST_ASSERT_EQUAL_PTR(data, array.data);
    TEST_ASSERT_EQUAL(len, array.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, array.data, len);
}

void test_ByteArray_CreateWithCString_should_create_ByteArray_using_C_string(void)
{
    char* str = "some string";
    size_t len = strlen(str);

    ByteArray array = ByteArray_CreateWithCString(str);

    TEST_ASSERT_EQUAL_PTR(str, array.data);
    TEST_ASSERT_EQUAL(len, array.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(str, array.data, len);
}


void test_ByteArray_FillWithDummyData_should_fill_an_array_with_dummy_data(void)
{
    uint8_t data[4];
    const size_t len = sizeof(data);
    memset(data, 0xFFu, len);
    ByteArray array = {.data = data, .len = len};
    uint8_t expected[] = {0x00, 0x01, 0x02, 0x03};

    ByteArray_FillWithDummyData(array);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, array.data, len);
}

void test_ByteArray_GetSlice_should_return_a_subarray_of_the_ByteArray(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u};
    size_t len = sizeof(data);
    ByteArray array = {.data = data, .len = len};
    size_t sliceLen;

    // Validate slice of full array
    ByteArray slice0 = ByteArray_GetSlice(array, 0, len);
    TEST_ASSERT_EQUAL_PTR(data, slice0.data);
    TEST_ASSERT_EQUAL(len, slice0.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, slice0.data, len);

    // Validate slice from beginning of array
    sliceLen = len - 1;
    ByteArray slice1 = ByteArray_GetSlice(array, 0, sliceLen);
    TEST_ASSERT_EQUAL_PTR(data, slice1.data);
    TEST_ASSERT_EQUAL(sliceLen, slice1.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, slice1.data, sliceLen);

    // Validate slice from non-start to the end of the array
    sliceLen = len - 1;
    ByteArray slice2 = ByteArray_GetSlice(array, 1, sliceLen);
    TEST_ASSERT_EQUAL_PTR(&data[1], slice2.data);
    TEST_ASSERT_EQUAL(sliceLen, slice2.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&data[1], slice2.data, sliceLen);

    // Validate slice from middle of the array
    sliceLen = len - 2;
    ByteArray slice3 = ByteArray_GetSlice(array, 1, sliceLen);
    TEST_ASSERT_EQUAL_PTR(&data[1], slice3.data);
    TEST_ASSERT_EQUAL(sliceLen, slice3.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&data[1], slice3.data, sliceLen);
}

void test_ByteBuffer_Create_should_create_an_empty_ByteBuffer_with_specified_array_and_max_length(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u};
    size_t len = sizeof(data);

    ByteBuffer buffer = ByteBuffer_Create(data, len);

    TEST_ASSERT_EQUAL_PTR(data, buffer.array.data);
    TEST_ASSERT_EQUAL(len, buffer.array.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, buffer.array.data, len);
    TEST_ASSERT_EQUAL(0, buffer.bytesUsed);
}

void test_ByteBuffer_CreateWithArray_should_create_an_empty_ByteBuffer_with_specified_ByteArray(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u};
    size_t len = sizeof(data);
    ByteArray array = {.data = data, .len = len};

    ByteBuffer buffer = ByteBuffer_CreateWithArray(array);

    TEST_ASSERT_EQUAL_PTR(data, buffer.array.data);
    TEST_ASSERT_EQUAL(len, buffer.array.len);
    TEST_ASSERT_EQUAL_PTR(array.data, buffer.array.data);
    TEST_ASSERT_EQUAL(array.len, buffer.array.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, buffer.array.data, len);
    TEST_ASSERT_EQUAL(0, buffer.bytesUsed);
}

void test_ByteBuffer_BytesRemaining_should_return_lenth_of_unconsumed_bytes(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u};
    size_t len = sizeof(data);
    ByteArray array = {.data = data, .len = len};

    ByteBuffer buffer = ByteBuffer_CreateWithArray(array);

    TEST_ASSERT_EQUAL(len, ByteBuffer_BytesRemaining(buffer));

    buffer.bytesUsed = 3;
    TEST_ASSERT_EQUAL(1, ByteBuffer_BytesRemaining(buffer));

    buffer.bytesUsed = len;
    TEST_ASSERT_EQUAL(0, ByteBuffer_BytesRemaining(buffer));
}

void test_ByteBuffer_Reset_should_reset_bytes_used(void)
{
    ByteBuffer buffer = {.bytesUsed = 4};
    ByteBuffer_Reset(&buffer);
    TEST_ASSERT_EQUAL(0, buffer.bytesUsed);
}

void test_ByteBuffer_Consume(void)
{
    uint8_t data[] = {0x1u, 0x2u, 0x3u, 0x4u, 0x5u};
    size_t len = sizeof(data);
    size_t totalConsumed = 0;
    size_t lenToConsume;
    ByteArray array = {.data = data, .len = len};

    ByteBuffer buffer = ByteBuffer_CreateWithArray(array);

    lenToConsume = 2;
    totalConsumed += lenToConsume;
    ByteArray consumed0 = ByteBuffer_Consume(&buffer, lenToConsume);
    TEST_ASSERT_EQUAL_PTR(array.data, consumed0.data);
    TEST_ASSERT_EQUAL(totalConsumed, consumed0.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&data[0], consumed0.data, totalConsumed);
    TEST_ASSERT_EQUAL(totalConsumed, buffer.bytesUsed);

    lenToConsume = 1;
    totalConsumed += lenToConsume;
    ByteArray consumed1 = ByteBuffer_Consume(&buffer, lenToConsume);
    TEST_ASSERT_EQUAL_PTR(&array.data[2], consumed1.data);
    TEST_ASSERT_EQUAL(lenToConsume, consumed1.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&data[2], consumed1.data, lenToConsume);
    TEST_ASSERT_EQUAL(totalConsumed, buffer.bytesUsed);

    lenToConsume = 2;
    totalConsumed += lenToConsume;
    ByteArray consumed2 = ByteBuffer_Consume(&buffer, lenToConsume);
    TEST_ASSERT_EQUAL_PTR(&array.data[3], consumed2.data);
    TEST_ASSERT_EQUAL(lenToConsume, consumed2.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&data[3], consumed2.data, lenToConsume);
    TEST_ASSERT_EQUAL(totalConsumed, buffer.bytesUsed);
    TEST_ASSERT_EQUAL(len, buffer.bytesUsed);

    // Try to consume from a fully consumed buffer
    lenToConsume = 1;
    ByteArray consumed3 = ByteBuffer_Consume(&buffer, lenToConsume);
    TEST_ASSERT_EQUAL(0, consumed3.len);
    TEST_ASSERT_NULL(consumed3.data);
    TEST_ASSERT_EQUAL(totalConsumed, buffer.bytesUsed);
    TEST_ASSERT_EQUAL(len, buffer.bytesUsed);
}

void test_ByteBuffer_Append_should_append_bytes_to_the_buffer(void)
{
    uint8_t data[] = {0, 0, 0};
    size_t len = sizeof(data);
    ByteBuffer buffer = ByteBuffer_Create(data, len);
    uint8_t appendData[len];

    appendData[0] = 0xFCu;
    TEST_ASSERT_TRUE(ByteBuffer_Append(&buffer, appendData, 1));
    TEST_ASSERT_EQUAL(1, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 1);

    appendData[1] = 0xABu;
    appendData[2] = 0x8Cu;
    TEST_ASSERT_TRUE(ByteBuffer_Append(&buffer, &appendData[1], 2));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 3);

    // Ensure we can't append too much data!
    TEST_ASSERT_FALSE(ByteBuffer_Append(&buffer, &appendData[1], 2));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 3);
}

void test_ByteBuffer_AppendArray_should_append_an_array_to_the_buffer(void)
{
    uint8_t data[] = {0, 0, 0};
    size_t len = sizeof(data);
    ByteBuffer buffer = ByteBuffer_Create(data, len);
    uint8_t appendData[len];

    appendData[0] = 0xFCu;
    ByteArray appendArray0 = (ByteArray) {
        .data = appendData, .len = 1
    };
    TEST_ASSERT_TRUE(ByteBuffer_AppendArray(&buffer, appendArray0));
    TEST_ASSERT_EQUAL(1, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 1);

    appendData[1] = 0xABu;
    appendData[2] = 0x8Cu;
    TEST_ASSERT_TRUE(ByteBuffer_Append(&buffer, &appendData[1], 2));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 3);

    // Ensure we can't append too much data!
    TEST_ASSERT_FALSE(ByteBuffer_Append(&buffer, &appendData[1], 2));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appendData, buffer.array.data, 3);
}

void test_ByteBuffer_AppendCString_should_append_a_C_string(void)
{
    uint8_t data[] = {0, 0, 0};
    size_t len = sizeof(data);
    ByteBuffer buffer = ByteBuffer_Create(data, len);

    TEST_ASSERT_TRUE(ByteBuffer_AppendCString(&buffer, "Co"));
    TEST_ASSERT_EQUAL(2, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY("Co", buffer.array.data, 2);

    TEST_ASSERT_TRUE(ByteBuffer_AppendCString(&buffer, "d"));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY("Cod", buffer.array.data, 3);

    // Ensure we can't append too much data!
    TEST_ASSERT_FALSE(ByteBuffer_AppendCString(&buffer, "!"));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY("Cod", buffer.array.data, 3);
}

void test_ByteBuffer_AppendDummyData_should_append_dummy_data_to_buffer(void)
{
    uint8_t data[] = {5, 5, 5};
    size_t len = sizeof(data);
    ByteBuffer buffer = ByteBuffer_Create(data, len);
    uint8_t expectedData[] = {0, 1, 0};

    TEST_ASSERT_EQUAL(0, buffer.bytesUsed);

    TEST_ASSERT_TRUE(ByteBuffer_AppendDummyData(&buffer, 2));
    TEST_ASSERT_EQUAL(2, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expectedData, buffer.array.data, 2);

    TEST_ASSERT_TRUE(ByteBuffer_AppendDummyData(&buffer, 1));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expectedData, buffer.array.data, 3);

    // Ensure we can't append too much data!
    TEST_ASSERT_FALSE(ByteBuffer_AppendDummyData(&buffer, 1));
    TEST_ASSERT_EQUAL(3, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expectedData, buffer.array.data, 3);
}
