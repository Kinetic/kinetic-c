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

#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "kinetic_types_internal.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"

extern int KineticLogLevel;

void setUp(void)
{
    DELETE_FILE(TEST_LOG_FILE);
    KineticLogger_Init(NULL, -1);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticLogger_KINETIC_LOG_FILE_should_be_defined_properly(void)
{
    TEST_ASSERT_EQUAL_STRING("kinetic.log", KINETIC_LOG_FILE);
}

void test_KineticLogger_Init_should_be_disabled_if_logFile_is_NULL(void)
{
    KineticLogger_Init(NULL, 3);
    TEST_ASSERT_EQUAL(-1, KineticLogLevel);
    KineticLogger_Log(0, "This message should be discarded and not logged!");
}

void test_KineticLogger_Init_should_initialize_the_logger_with_specified_output_file(void)
{
    KineticLogger_Init(TEST_LOG_FILE, 3);
    KineticLogger_Log(0, "Some message to log file...");
    TEST_ASSERT_FILE_EXISTS(TEST_LOG_FILE);
    TEST_ASSERT_EQUAL(3, KineticLogLevel);
}

void test_KineticLogger_Init_should_log_to_stdout_if_specified(void)
{
    KineticLogger_Init("stdout", 0);
    TEST_ASSERT_EQUAL(0, KineticLogLevel);
    KineticLogger_Log(0, "This message should be logged to stdout!");
}

void test_KineticLogger_Log_should_write_log_message_to_file(void)
{
    const char* msg = "Some really important message!";
    KineticLogger_Init(TEST_LOG_FILE, 3);
    KineticLogger_LogPrintf(0, msg);
    // TEST_ASSERT_EQUAL_FILE_CONTENT(TEST_LOG_FILE, content, length);
    TEST_ASSERT_FILE_EXISTS(TEST_LOG_FILE);
}

void test_LOG_LOCATION_should_log_location(void)
{
    KineticLogger_Init(TEST_LOG_FILE, 2);
}
