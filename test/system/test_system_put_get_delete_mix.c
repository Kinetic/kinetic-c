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
#include "kinetic_client.h"
#include "system_test_kv_generate.h"

static const unsigned int KV_PAIRS_PER_GROUP = 9;
static const unsigned int TOTAL_GROUPS = 10;
static const int DEFAULT_BUFFER_SIZE = 1024;

static ByteBuffer ExpectedTagBuffer;
static ByteBuffer ExpectedVersionBuffer;

void setUp(void)
{
    SystemTestSetup(1, true);
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_put_get_delete_one_entry_by_one_entry(void)
{
    uint8_t version_data[DEFAULT_BUFFER_SIZE], tag_data[DEFAULT_BUFFER_SIZE], value_data[DEFAULT_BUFFER_SIZE];
    ByteBuffer version_buffer, tag_buffer, value_buffer;
    version_buffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    ExpectedVersionBuffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    tag_buffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");
    ExpectedTagBuffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");
    value_buffer = ByteBuffer_Create(value_data, DEFAULT_BUFFER_SIZE, 0);

    unsigned int i;
    for (i=0; i<KV_PAIRS_PER_GROUP; i++)
    {
    	// put object
        KineticEntry putEntry = {
   	       .key = generate_entry_key_by_index(i),
   	       .tag = tag_buffer,
   	       .newVersion = version_buffer,
           .algorithm = KINETIC_ALGORITHM_SHA1,
           .value = generate_entry_value_by_index(i),
           .force = true,
           .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };

        KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

        ByteBuffer_Reset(&value_buffer);

        // get object
		value_buffer = ByteBuffer_Create(value_data, DEFAULT_BUFFER_SIZE, 0);
		KineticEntry getEntry = {
	        .key = generate_entry_key_by_index(i),
			.dbVersion = version_buffer,
	        .tag = tag_buffer,
			.value = value_buffer,
	    };

	    status = KineticClient_Get(Fixture.session, &getEntry, NULL);

	    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
	    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
	    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_key_by_index(i), getEntry.key);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
	    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
	    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_value_by_index(i), getEntry.value);

	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, version_buffer);
   	    // delete object
	    KineticEntry deleteEntry = {
	        .key = generate_entry_key_by_index(i),
			.dbVersion = version_buffer,
	    };
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, version_buffer);
	    status = KineticClient_Delete(Fixture.session, &deleteEntry, NULL);
	    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
	    TEST_ASSERT_EQUAL(0, deleteEntry.value.bytesUsed);

   	    // get object again
     	status = KineticClient_Get(Fixture.session, &getEntry, NULL);
     	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    }
}

void test_put_get_delete_one_group_by_one_group(void)
{
    uint8_t version_data[DEFAULT_BUFFER_SIZE], tag_data[DEFAULT_BUFFER_SIZE], value_data[DEFAULT_BUFFER_SIZE];
    ByteBuffer version_buffer, tag_buffer, value_buffer;
    version_buffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    ExpectedVersionBuffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    tag_buffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");
    ExpectedTagBuffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");
    value_buffer = ByteBuffer_Create(value_data, DEFAULT_BUFFER_SIZE, 0);

    unsigned int i, j;
    for (i =0; i<TOTAL_GROUPS; i++)
    {
    	KineticStatus status;

    	// put a group of entries
        for (j=0; j<KV_PAIRS_PER_GROUP; j++)
        {
            KineticEntry putEntry = {
       	       .key = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j),
       	       .tag = tag_buffer,
       	       .newVersion = version_buffer,
               .algorithm = KINETIC_ALGORITHM_SHA1,
               .value = generate_entry_value_by_index(i*KV_PAIRS_PER_GROUP + j),
               .force = true,
               .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
            };

            status = KineticClient_Put(Fixture.session, &putEntry, NULL);
            TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        }

        // get key_range
        KineticKeyRange range = {
            .startKey = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP),
            .endKey = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + KV_PAIRS_PER_GROUP -1),
            .startKeyInclusive = true,
            .endKeyInclusive = true,
            .maxReturned = KV_PAIRS_PER_GROUP,
        };

        ByteBuffer keyBuff[KV_PAIRS_PER_GROUP];
        uint8_t keysData[KV_PAIRS_PER_GROUP][DEFAULT_BUFFER_SIZE];
        for (j = 0; j < KV_PAIRS_PER_GROUP; j++) {
            memset(&keysData[j], 0, DEFAULT_BUFFER_SIZE);
            keyBuff[j] = ByteBuffer_Create(&keysData[j], DEFAULT_BUFFER_SIZE, 0);
        }
        ByteBufferArray keys = {.buffers = keyBuff, .count = KV_PAIRS_PER_GROUP, .used = 0};

        status = KineticClient_GetKeyRange(Fixture.session, &range, &keys, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        TEST_ASSERT_EQUAL(KV_PAIRS_PER_GROUP, keys.used);
        for (j = 0; j < KV_PAIRS_PER_GROUP; j++)
        {
        	TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j), keys.buffers[j]);
        }

        // delete a group of entries
        for (j=0; j<KV_PAIRS_PER_GROUP; j++)
        {
        	ByteBuffer_Reset(&value_buffer);

   	        // get object
   			value_buffer = ByteBuffer_Create(value_data, DEFAULT_BUFFER_SIZE, 0);
   			KineticEntry getEntry = {
   		        .key = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j),
   				.dbVersion = version_buffer,
   		        .tag = tag_buffer,
   				.value = value_buffer,
   		    };

   		    status = KineticClient_Get(Fixture.session, &getEntry, NULL);

   		    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
   		    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
   		    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
   		    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j), getEntry.key);
   		    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
   		    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
   		    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_value_by_index(i*KV_PAIRS_PER_GROUP + j), getEntry.value);
   		    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, version_buffer);
        }

        // delete a group of entries
        for (j=0; j<KV_PAIRS_PER_GROUP; j++)
        {
        	   	    // delete object
  		    KineticEntry deleteEntry = {
   		        .key = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j),
   				.dbVersion = version_buffer,
   		    };
   		    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, version_buffer);
   		    status = KineticClient_Delete(Fixture.session, &deleteEntry, NULL);
   		    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
   		    TEST_ASSERT_EQUAL(0, deleteEntry.value.bytesUsed);

   	   	    // get object again
        	ByteBuffer_Reset(&value_buffer);

   	        // get object
   			value_buffer = ByteBuffer_Create(value_data, DEFAULT_BUFFER_SIZE, 0);
   			KineticEntry getEntry = {
   		        .key = generate_entry_key_by_index(i*KV_PAIRS_PER_GROUP + j),
   				.dbVersion = version_buffer,
   		        .tag = tag_buffer,
   				.value = value_buffer,
   		    };
   	     	status = KineticClient_Get(Fixture.session, &getEntry, NULL);
   	     	TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
        }
    }
}

