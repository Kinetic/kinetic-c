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

#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"

extern int KineticLogLevel;

void setUp(void)
{
    DELETE_FILE(TEST_LOG_FILE);
    KineticLogger_Init(NULL);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticLogger_KINETIC_LOG_FILE_should_be_defined_properly(void)
{ LOG_LOCATION;
    TEST_ASSERT_EQUAL_STRING("kinetic.log", KINETIC_LOG_FILE);
}

void test_KineticLogger_Init_should_be_disabled_if_logFile_is_NULL(void)
{ LOG_LOCATION;
    KineticLogger_Init(NULL);
    TEST_ASSERT_EQUAL(-1, KineticLogLevel);
    KineticLogger_Log(0, "This message should be discarded and not logged!");
}

void test_KineticLogger_Init_should_initialize_the_logger_with_specified_output_file(void)
{ LOG_LOCATION;
    KineticLogger_Init(TEST_LOG_FILE);
    KineticLogger_Log(0, "Some message to log file...");
    TEST_ASSERT_FILE_EXISTS(TEST_LOG_FILE);
    TEST_ASSERT_EQUAL(0, KineticLogLevel);
}

void test_KineticLogger_Init_should_log_to_stdout_if_specified(void)
{ LOG_LOCATION;
    KineticLogger_Init("stdout");
    TEST_ASSERT_EQUAL(0, KineticLogLevel);
    KineticLogger_Log(0, "This message should be logged to stdout!");
}

void test_KineticLogger_Log_should_write_log_message_to_file(void)
{ LOG_LOCATION;
    const char* msg = "Some really important message!";
    KineticLogger_Init(TEST_LOG_FILE);
    KineticLogger_LogPrintf(0, msg);
    // TEST_ASSERT_EQUAL_FILE_CONTENT(TEST_LOG_FILE, content, length);
    TEST_ASSERT_FILE_EXISTS(TEST_LOG_FILE);
}

void test_LOG_LOCATION_should_log_location(void)
{
    KineticLogger_Init(TEST_LOG_FILE);
    LOG_LOCATION;
}
