/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
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

#include "kinetic_serial_allocator.h"
#include "kinetic_types_internal.h"
#include "kinetic_types.h"
#include "kinetic_logger.h"
#include "byte_array.h"
#include "kinetic_proto.h"
#include "protobuf-c.h"
#include "unity.h"
#include "unity_helper.h"
#include <stdlib.h>

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}


void test_KineticSerialAllocator_Create_should_allocate_a_new_buffer_and_return_configured_allocator_instance(void)
{
    const size_t maxLen = 17;
    KineticSerialAllocator allocator = KineticSerialAllocator_Create(maxLen);

    TEST_ASSERT_EQUAL_SIZET(maxLen, allocator.total);
    TEST_ASSERT_NOT_NULL(allocator.buffer);
    TEST_ASSERT_EQUAL_SIZET(0, allocator.used);

    // Ensure we can write to all locations in the allocated buffer
    memset(allocator.buffer, 0x5Au, maxLen);
}



void test_KineticSerialAllocator_AllocateChunk_should_allocate_new_chunks_of_data_from_end_of_buffer(void)
{
    const size_t maxLen = (sizeof(uint64_t) * 4) + 2;
    KineticSerialAllocator allocator = KineticSerialAllocator_Create(maxLen);

    // Allocate 1 byte
    char* ch = KineticSerialAllocator_AllocateChunk(&allocator, 1);
    TEST_ASSERT_EQUAL_PTR(allocator.buffer, ch);
    *ch = '!';
    TEST_ASSERT_EQUAL_SIZET(sizeof(uint64_t), allocator.used);

    // Allocate a uint64_t (8 bytes)
    uint64_t* u64 = KineticSerialAllocator_AllocateChunk(&allocator, sizeof(uint64_t));
    TEST_ASSERT_EQUAL_PTR(&allocator.buffer[sizeof(uint64_t)], u64);
    *u64 = 0x0123456789ABCDEFu;
    TEST_ASSERT_EQUAL_SIZET(2 * sizeof(uint64_t), allocator.used);

    // Allocate 1 byte less than uint64_t (7 bytes)
    uint8_t* u8 = KineticSerialAllocator_AllocateChunk(&allocator, sizeof(uint64_t) - 1);
    TEST_ASSERT_EQUAL_PTR(&allocator.buffer[sizeof(uint64_t) * 2], u8);
    memset(u8, 0x7Cu, sizeof(uint64_t) - 1);
    TEST_ASSERT_EQUAL_SIZET(3 * sizeof(uint64_t), allocator.used);

    // Allocate 1 byte more than a uint64_t (9 bytes)
    u8 = KineticSerialAllocator_AllocateChunk(&allocator, sizeof(uint64_t) + 1);
    TEST_ASSERT_EQUAL_PTR(&allocator.buffer[sizeof(uint64_t) * 3], u8);
    TEST_ASSERT_EQUAL_SIZET(maxLen, allocator.used);
    memset(u8, 0xB3u, sizeof(uint64_t) + 1);

    // Try to allocate another chunk (e.g. 1 byte), which should fail, since full
    u8 = KineticSerialAllocator_AllocateChunk(&allocator, 1);
    TEST_ASSERT_NULL(u8);
    TEST_ASSERT_EQUAL_SIZET(maxLen, allocator.used);
}
