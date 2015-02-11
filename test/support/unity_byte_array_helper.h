/*
* kinetic-c-client
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
#ifndef _UNITY_BYTE_ARRAY_HELPER
#define _UNITY_BYTE_ARRAY_HELPER

#include "unity.h"
#include <stdio.h>
#include <string.h>


/** Custom Unity assertions for validating equality of size_t values */
#ifndef TEST_ASSERT_EQUAL_SIZET_MESSAGE
#define TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, actual, msg);
#endif
#ifndef TEST_ASSERT_EQUAL_SIZET
#define TEST_ASSERT_EQUAL_SIZET(expected, actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, NULL);
#endif


/** Custom Unity/CMock equality assertion for validating equality of ByteArrays */
#define TEST_ASSERT_EQUAL_ByteArray_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT_MESSAGE((expected).len, (actual).len, "Lengths do not match!"); \
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE((expected).data, (actual).data, (expected).len, "Data does not match!");
#define TEST_ASSERT_EQUAL_ByteArray(expected, actual) \
    TEST_ASSERT_EQUAL_ByteArray_MESSAGE(expected, actual, NULL);

/** Custom Unity assertion for validating a ByteArray is empty (zero length) */
#define TEST_ASSERT_ByteArray_EMPTY(_actual) \
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (_actual).len, "ByteArray length is non-zero!");

/** Custom Unity assertion for validating a ByteArray is unallocated (zero length w/NULL buffer) */
#define TEST_ASSERT_ByteArray_NONE(_actual) \
    TEST_ASSERT_ByteArray_EMPTY(_actual); \
    TEST_ASSERT_NULL_MESSAGE((_actual).data, "ByteArray has non-null buffer");

/** Custom Unity/CMock equality assertion for validating equality of ByteBuffers */
#define TEST_ASSERT_EQUAL_ByteBuffer(_expected, _actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE((_expected).bytesUsed, (_actual).bytesUsed, \
        "ByteBuffer used lengths do not match!"); \
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE((_expected).array.data, \
        ((_actual)).array.data, (_expected).bytesUsed, \
        "ByteBuffer data does not match!");

/** Custom Unity assertion for validating a ByteBuffer is empty (zero length) */
#define TEST_ASSERT_ByteBuffer_EMPTY(_actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(0, \
        (_actual).bytesUsed, "ByteBuffer bytes used greater than zero!");

/** Custom Unity assertion for validating a ByteArray is NULL  */
#define TEST_ASSERT_ByteBuffer_NULL(_actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(0, (_actual).array.len, \
        "ByteBuffer array was not empty!"); \
    TEST_ASSERT_NULL_MESSAGE((_actual).array.data, \
        "ByteBuffer array was not NULL!"); \
    TEST_ASSERT_ByteBuffer_EMPTY((_actual));

/** Custom Unity/CMock equality assertion for validating equality of a ByteArray w/ a ByteBuffer */
#define TEST_ASSERT_ByteArray_EQUALS_ByteBuffer(_array, _buf) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE((_array).len, (_buf).bytesUsed, \
        "Lengths do not match!"); \
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE((_array).data, \
        (_buf).array.data, (_buf).bytesUsed, \
        "Data does not match!");

#endif // _UNITY_BYTE_ARRAY_HELPER
