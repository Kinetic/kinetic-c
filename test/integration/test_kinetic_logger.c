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

#include "unity_helper.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
// #include "zlog/zlog.h"

extern bool LogToConsole;
extern FILE* FileDesc;

void setUp(void)
{
    DELETE_FILE(TEST_LOG_FILE);
}

void tearDown(void)
{
    KineticLogger_Close();
    DELETE_FILE(TEST_LOG_FILE);
}

void test_KineticLogger_KINETIC_LOG_FILE_should_be_defined_properly(void)
{
    TEST_ASSERT_EQUAL_STRING("kinetic.log", KINETIC_LOG_FILE);
}

void test_KineticLogger_Init_should_log_to_STDOUT_by_default(void)
{
    KineticLogger_Init(NULL);

    TEST_ASSERT_TRUE(LogToConsole);
    TEST_ASSERT_NULL(FileDesc);
}

void test_KineticLogger_Init_should_initialize_the_logger_with_specified_output_file(void)
{
    KineticLogger_Init(TEST_LOG_FILE);

    TEST_ASSERT_FALSE(LogToConsole);
    TEST_ASSERT_FILE_EXISTS(TEST_LOG_FILE);

}

void test_KineticLogger_Log_should_write_log_message_to_file(void)
{
    const char* msg = "Some really important message!";
    char content[64];
    size_t length;

    sprintf(content, "%s\n", msg);
    length = strlen(content);

    KineticLogger_Init(TEST_LOG_FILE);

    KineticLogger_Log(msg);

    TEST_ASSERT_EQUAL_FILE_CONTENT(TEST_LOG_FILE, content, length);
}
