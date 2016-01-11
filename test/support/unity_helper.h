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

#ifndef _UNITY_HELPER
#define _UNITY_HELPER

#include "unity.h"
#include "unity_byte_array_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_countingsemaphore_types.h"
#include "bus_internal_types.h"
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

/** Custom Unity assertion which validates a size_t value */
#define TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, msg) \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, actual, msg);
#define TEST_ASSERT_EQUAL_SIZET(expected, actual) \
    TEST_ASSERT_EQUAL_SIZET_MESSAGE(expected, actual, NULL);


/** Custom Unity assertion which validates a the value of a Kinetic protocol status code */
#define TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(expected, actual, msg) \
if ((expected) != (actual)) { \
    char err[256]; \
    const char* invalidStatus = "INVALID_STATUS"; \
    const char* statusDescExpected = invalidStatus; \
    if ((expected) >= 0 && (expected) < KINETIC_STATUS_COUNT) { \
        statusDescExpected = Kinetic_GetStatusDescription(expected); } \
    const char* statusDescActual = invalidStatus; \
    if ((actual) >= 0 && (actual) < KINETIC_STATUS_COUNT) { \
        statusDescActual = Kinetic_GetStatusDescription(actual); } \
    sprintf(err, "Expected Kinetic status code of %s(%d), Was %s(%d) ", \
        statusDescExpected, (expected), statusDescActual, (actual)); \
    char * p_msg = msg; \
    if (p_msg != NULL) { strcat(err, " : "); strcat(err, p_msg); } \
    TEST_FAIL_MESSAGE(err); \
}
#define TEST_ASSERT_EQUAL_KineticStatus(expected, actual) \
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(expected, actual, NULL);

/** Custom Unity assertion which validates the expected int64_t value is
    packed properly into a buffer in Network Byte-Order (big endian) */
#define TEST_ASSERT_EQUAL_uint32_nbo_t(expected, buf) { \
    size_t i = 0; int32_t val = 0; char err[64]; uint8_t* p = (uint8_t*)&buf;\
    for(; i < sizeof(int32_t); i++) {val <<= 8; val += p[i]; } \
    sprintf(err, "@ index %zu", i); \
    TEST_ASSERT_EQUAL_INT32_MESSAGE(expected, val, err); \
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

#endif // _UNITY_HELPER
