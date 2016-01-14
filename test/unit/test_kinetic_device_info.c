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

#include "kinetic_device_info.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include "unity_helper.h"
#include <stdlib.h>

static KineticLogInfo* Info;

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


void test_KineticLogInfo_Create_should_allocate_and_populate_device_info_with_utilizations(void)
{
    size_t expectedSize = sizeof(KineticLogInfo);
    const int numUtilizations = 2;
    Com__Seagate__Kinetic__Proto__Command__GetLog__Utilization utilizations[] = {
        COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__UTILIZATION__INIT,
        COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__UTILIZATION__INIT,
    };
    Com__Seagate__Kinetic__Proto__Command__GetLog getLog = COM__SEAGATE__KINETIC__PROTO__COMMAND__GET_LOG__INIT;
    getLog.n_utilizations = numUtilizations;
    char* names[] = {"fo", "shizzle"};

    utilizations[0].name = names[0];
    utilizations[0].has_value = true;
    utilizations[0].value = 1.7f;
    expectedSize += sizeof(KineticLogInfo_Utilization) + GetPaddedLength(strlen(utilizations[0].name)+1);

    utilizations[1].name = names[1];
    utilizations[1].has_value = true;
    utilizations[1].value = 2.3f;
    expectedSize += sizeof(KineticLogInfo_Utilization) + GetPaddedLength(strlen(utilizations[1].name)+1);

    Com__Seagate__Kinetic__Proto__Command__GetLog__Utilization* pUtilizations[] = {
        &utilizations[0], &utilizations[1]
    };
    getLog.utilizations = pUtilizations;

    Info = KineticLogInfo_Create(&getLog);

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
