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

#include "system_test_fixture.h"
#include "kinetic_admin_client.h"
#include "system_test_kv_generate.h"

static const unsigned int TOTAL_PUT_KEYS = 10;
static const int DEFAULT_SIZE = 1024;

void setUp(void)
{
    SystemTestSetup(1, false);
    uint8_t version_data[DEFAULT_SIZE], tag_data[DEFAULT_SIZE];
    ByteBuffer version_buffer, tag_buffer;
    version_buffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    tag_buffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");

    ByteBuffer key_buffers[TOTAL_PUT_KEYS];
    ByteBuffer value_buffers[TOTAL_PUT_KEYS];
    unsigned int i;
    for (i=0; i<TOTAL_PUT_KEYS; i++)
    {
    	key_buffers[i] = generate_entry_key_by_index(i);

    	printf("key: %s", (char *)key_buffers[i].array.data);
    	value_buffers[i] = generate_entry_value_by_index(i);

        KineticEntry putEntry = {
   	       .key = key_buffers[i],
   	       .tag = tag_buffer,
   	       .newVersion = version_buffer,
           .algorithm = KINETIC_ALGORITHM_SHA1,
           .value = value_buffers[i],
           .force = true,
           .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };

        KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

        ByteBuffer_Free(key_buffers[i]);
        ByteBuffer_Free(value_buffers[i]);
    }
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_media_scan_should_succeed_for_existing_key_range(void)
{
	KineticMediaScan_Operation mediascan_operation = {"my_key         1","my_key         5", true, true};
	KineticCommand_Priority priority = PRIORITY_NORMAL;

	KineticStatus status = KineticAdminClient_MediaScan(Fixture.session, &mediascan_operation, priority);
	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_media_optimize_should_succeed_for_existing_key_range(void)
{
    KineticMediaOptimize_Operation mediaoptimize_operation = {"my_key         6","my_key         9", true, true};
	KineticCommand_Priority priority = PRIORITY_NORMAL;

	KineticStatus status = KineticAdminClient_MediaOptimize(Fixture.session, &mediaoptimize_operation, priority);
	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
