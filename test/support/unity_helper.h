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

#include "unity.h"
#include <stdio.h>
#include <string.h>

#define TEST_LOG_FILE "build/artifacts/test/test.log"

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

/** Custom Unity assertion which validates a size_t value */
#define TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, actual, msg);
#define TEST_ASSERT_EQUAL_SIZET(expected, actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, NULL);

/** Custom Unity assertion which validates a the value of a Kinetic protocol status code */
#define TEST_ASSERT_EQUAL_KINETIC_STATUS(expected, actual) \
if (expected != actual) { \
    char err[128]; \
    sprintf(err, "Expected Kinetic status code of %s(%d), Was %s(%d)", \
        protobuf_c_enum_descriptor_get_value( \
            &KineticProto_status_status_code_descriptor, expected)->name, \
        expected, \
        protobuf_c_enum_descriptor_get_value( \
            &KineticProto_status_status_code_descriptor, actual)->name, \
        actual); \
    TEST_FAIL_MESSAGE(err); \
}

/** Custom Unity assertion which validates the expected int64_t value is
    packed properly into a buffer in Network Byte-Order (big endian) */
#define TEST_ASSERT_EQUAL_NBO_INT64(expected, buf) { \
    int i; int64_t val = 0; char err[64];\
    for(i = 0; i < sizeof(int64_t); i++) {val <<= 8; val += buf[i]; } \
    sprintf(err, "@ index %d", i); \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, val, err); \
}

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


