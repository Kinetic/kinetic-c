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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#ifndef _UNITY_HELPER
#define _UNITY_HELPER

#include "unity.h"
#include "kinetic_types.h"
#include <stdio.h>
#include <string.h>

#define TEST_LOG_FILE "./build/artifacts/test/test.log"

// Load file access library to get cross-platform ability to delete a file
#ifdef WIN32
    #include <io.h>
    #define FACCESS _access
#else
    #if defined(__unix__) || defined(__APPLE__)
        #include <unistd.h>
        #define FACCESS access
    #else
        #error Failed to find required file IO library with file access() method
    #endif
#endif // WIN32



// // Make a FOREACH macro
// #define FE_1(WHAT, X) WHAT(X) 
// #define FE_2(WHAT, X, ...) WHAT(X)FE_1(WHAT, __VA_ARGS__)
// #define FE_3(WHAT, X, ...) WHAT(X)FE_2(WHAT, __VA_ARGS__)
// #define FE_4(WHAT, X, ...) WHAT(X)FE_3(WHAT, __VA_ARGS__)
// #define FE_5(WHAT, X, ...) WHAT(X)FE_4(WHAT, __VA_ARGS__)
// //... repeat as needed

// #define GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME 
// #define FOR_EACH(action,...) 
//   GET_MACRO(__VA_ARGS__,FE_5,FE_4,FE_3,FE_2,FE_1)(action,__VA_ARGS__)

// #define _APPEND_UNDERSCORE(X) _X
// // Helper function
// #define _UNDERSCORE(_first,...) 
//   _first ## _ ## FOR_EACH(_APPEND_UNDERSCORE,__VA_ARGS__)



// // Macro to define start of a Unity test method
// #define CONCAT(a,b) a_ ## b
// #define void test_desc(void)
// { void test_ ## desc ## _CASE(void) {LOG_LOCATION;


/** Custom Unity assertion which validates a size_t value */
#define TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, actual, msg);
#define TEST_ASSERT_EQUAL_SIZET(expected, actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, NULL);

/** Custom Unity assertion which validates a the value of a Kinetic protocol status code */
#define TEST_ASSERT_EQUAL_KINETIC_STATUS(expected, actual) \
if ((expected) != (actual)) { \
    char err[128]; \
    const char* invalidStatus = "INVALID_STATUS"; \
    const char* statusDescExpected = invalidStatus; \
    if ((expected) >= 0 && (expected) < KineticStatusDescriptorCount) { \
        statusDescExpected = KineticStatusDescriptor[(expected)]; } \
    const char* statusDescActual = invalidStatus; \
    if ((actual) >= 0 && (actual) < KineticStatusDescriptorCount) { \
        statusDescActual = KineticStatusDescriptor[(actual)]; } \
    sprintf(err, "Expected Kinetic status code of %s(%d), Was %s(%d)", \
        statusDescExpected, (expected), statusDescActual, (actual));\
    TEST_FAIL_MESSAGE(err); \
}

/** Custom Unity assertion which validates the expected int64_t value is
    packed properly into a buffer in Network Byte-Order (big endian) */
#define TEST_ASSERT_EQUAL_uint32_nbo_t(expected, buf) { \
    size_t i = 0; int32_t val = 0; char err[64]; uint8_t* p = (uint8_t*)&buf;\
    for(; i < sizeof(int32_t); i++) {val <<= 8; val += p[i]; } \
    sprintf(err, "@ index %zu", i); \
    TEST_ASSERT_EQUAL_INT32_MESSAGE(expected, val, err); \
}



/** Custom Unity/CMock equality assertion for validating equality of ByteArrays */
#define TEST_ASSERT_EQUAL_ByteArray_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT_MESSAGE((expected).len, (actual).len, "Lengths do not match!"); \
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE((expected).data, (actual).data, (expected).len, "Data does not match!");
#define TEST_ASSERT_EQUAL_ByteArray(expected, actual) \
    TEST_ASSERT_EQUAL_ByteArray_MESSAGE(expected, actual, NULL);

/** Custom Unity assertion for validating empty ByteArrays */
#define TEST_ASSERT_ByteArray_EMPTY(actual) \
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (actual).len, "ByteArray length is non-zero!");
#define TEST_ASSERT_ByteArray_NONE(actual) \
    TEST_ASSERT_ByteArray_EMPTY(actual); \
    TEST_ASSERT_NULL_MESSAGE((actual).data, "ByteArray has non-null buffer");



#define GET_CWD(cwd) \
    TEST_ASSERT_NOT_NULL_MESSAGE(getcwd(cwd, sizeof(cwd)), "Failed getting current working directory");

/** Custom Unity assertion which validates a file exists */
#define FILE_EXISTS(fname) (FACCESS(fname, F_OK) != -1)

/** Custom Unity helper which deletes a file if it exists */
#define DELETE_FILE(fname) if (FILE_EXISTS(fname)) \
    TEST_ASSERT_EQUAL_MESSAGE(0, remove(fname), \
        "Failed deleting file: " fname)

/** Custom Unity assertion which validates a file exists */
#define TEST_ASSERT_FILE_EXISTS(fname) \
    TEST_ASSERT_TRUE_MESSAGE(FILE_EXISTS(fname), "File does not exist: " #fname )

/** Custom Unity assertion which validates the contents of a file ('len' - length in bytes) */
#define TEST_ASSERT_EQUAL_FILE_CONTENT(fname, content, len) { \
    FILE* f; \
    char c, buff[len], err[1024 + len]; \
    size_t actualLen; \
    TEST_ASSERT_FILE_EXISTS(fname); \
    f = fopen(fname, "r"); \
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to open file for reading: " #fname ); \
    for (actualLen = 0; actualLen < len; actualLen++) { \
        c = fgetc(f); \
        if (c == EOF) { perror("EOF found!"); break; } \
        buff[actualLen] = c; } \
    buff[actualLen] = '\0'; /* Append NULL terminator */ \
    if (actualLen != len) { \
        TEST_ASSERT_NOT_EQUAL_MESSAGE(EOF, c, err); } \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(len, actualLen, "File read error (truncated?)!"); \
    TEST_ASSERT_EQUAL_STRING_MESSAGE(content, buff, "File contents did not match!"); \
    TEST_ASSERT_EQUAL_MESSAGE(0, fclose(f), "Failed closing file: " #fname ); \
}

#endif // _UNITY_HELPER
