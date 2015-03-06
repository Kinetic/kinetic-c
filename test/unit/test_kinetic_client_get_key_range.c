
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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_device_info.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_builder.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_bus.h"
#include "mock_kinetic_memory.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_resourcewaiter.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSession Session;
static const char* StartKeyData[KINETIC_DEFAULT_KEY_LEN];
static const char* EndKeyData[KINETIC_DEFAULT_KEY_LEN];
static ByteBuffer StartKey, EndKey;
#define MAX_KEYS_RETRIEVED (4)
static uint8_t KeyRangeData[MAX_KEYS_RETRIEVED][KINETIC_MAX_KEY_LEN];
static ByteBuffer Keys[MAX_KEYS_RETRIEVED];

void setUp(void)
{
    KineticLogger_Init("stdout", 3);

    // Configure start and end key buffers
    StartKey = ByteBuffer_Create(StartKeyData, sizeof(StartKeyData), sizeof(StartKeyData));
    EndKey = ByteBuffer_Create(EndKeyData, sizeof(EndKeyData), sizeof(EndKeyData));

    // Initialize buffers to hold returned keys in requested range
    for (int i = 0; i < MAX_KEYS_RETRIEVED; i++) {
        Keys[i] = ByteBuffer_Create(&KeyRangeData[i], sizeof(KeyRangeData[i]), sizeof(KeyRangeData[i]));
        char keyBuf[64];
        snprintf(keyBuf, sizeof(keyBuf), "key_range_00_%02d", i);
        ByteBuffer_AppendCString(&Keys[i], keyBuf);
    }
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_GetKeyRange_should_return_a_list_of_keys_within_the_specified_range(void)
{
    ByteBuffer_AppendCString(&StartKey, "key_range_00_00");
    ByteBuffer_AppendCString(&EndKey, "key_range_00_03");

    KineticKeyRange keyRange = {
        .startKey = StartKey,
        .endKey = EndKey,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = MAX_KEYS_RETRIEVED,
        .reverse = false,
    };
    ByteBufferArray keyArray = {
        .buffers = &Keys[0],
        .count = MAX_KEYS_RETRIEVED
    };
    KineticOperation operation;

    KineticAllocator_NewOperation_ExpectAndReturn(&Session, &operation);
    KineticBuilder_BuildGetKeyRange_ExpectAndReturn(&operation, &keyRange, &keyArray, KINETIC_STATUS_SUCCESS);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_BUFFER_OVERRUN);

    KineticStatus status = KineticClient_GetKeyRange(&Session, &keyRange, &keyArray, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);
}
