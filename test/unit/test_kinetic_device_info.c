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

#include "kinetic_device_info.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "kinetic_serial_allocator.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include "unity_helper.h"
#include <stdlib.h>

static KineticDeviceInfo* Info;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    Info = NULL;
}

void tearDown(void)
{
    // if (Info != NULL) {
    //     free(Info);
    //     Info = NULL;
    // }
    KineticLogger_Close();
}


static size_t GetPaddedLength(size_t len)
{
    size_t padding = (sizeof(uint64_t) - 1);
    size_t paddedLen = len + padding;
    paddedLen &= ~padding;
    return paddedLen;
}


void test_KineticDeviceInfo_Create_should_allocate_and_populate_device_info_with_utilizations(void)
{
    size_t expectedSize = sizeof(KineticDeviceInfo);
    const int numUtilizations = 2;
    KineticProto_Command_GetLog_Utilization utilizations[] = {
        KINETIC_PROTO_COMMAND_GET_LOG_UTILIZATION__INIT,
        KINETIC_PROTO_COMMAND_GET_LOG_UTILIZATION__INIT,
    };
    KineticProto_Command_GetLog getLog = KINETIC_PROTO_COMMAND_GET_LOG__INIT;
    getLog.n_utilizations = numUtilizations;
    char* names[] = {"fo", "shizzle"};
    
    utilizations[0].name = names[0];
    utilizations[0].has_value = true;
    utilizations[0].value = 1.7f;
    expectedSize += sizeof(KineticDeviceInfo_Utilization) + GetPaddedLength(strlen(utilizations[0].name)+1);

    utilizations[1].name = names[1];
    utilizations[1].has_value = true;
    utilizations[1].value = 2.3f;
    expectedSize += sizeof(KineticDeviceInfo_Utilization) + GetPaddedLength(strlen(utilizations[1].name)+1);

    KineticProto_Command_GetLog_Utilization* pUtilizations[] = {
        &utilizations[0], &utilizations[1]
    };
    getLog.utilizations = pUtilizations;

    Info = KineticDeviceInfo_Create(&getLog);

    TEST_ASSERT_NOT_NULL(Info);
    TEST_ASSERT_NOT_NULL(Info->utilizations);
    TEST_ASSERT_EQUAL(numUtilizations, Info->numUtilizations);

    TEST_ASSERT_EQUAL_STRING("fo", Info->utilizations[0].name);
    TEST_ASSERT_EQUAL_FLOAT(1.7f, Info->utilizations[0].value);

    TEST_ASSERT_EQUAL_STRING("shizzle", Info->utilizations[1].name);
    TEST_ASSERT_EQUAL_FLOAT(2.3f, Info->utilizations[1].value);

    free(Info);
    Info = NULL;
}
