/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "system_test_fixture.h"
#include "kinetic_client.h"

// Prepare data for foo entry
uint8_t foo_value_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
uint8_t foo_key_data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
ByteBuffer foo_value,foo_key,foo_tag;
uint8_t foo_sha1[20];
ByteBuffer foo_get_value_buf;
ByteBuffer foo_get_tag_buf;

// Prepare data for bar entry
uint8_t bar_value_data[] = { 0x01, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
uint8_t bar_key_data[] = {0x01, 0x00, 0x02, 0x03, 0x04};
ByteBuffer bar_value,bar_key,bar_tag;
uint8_t bar_sha1[20];
ByteBuffer bar_get_value_buf;
ByteBuffer bar_get_tag_buf;


KineticBatch_Operation * batchOp;

void setUp(void)
{
    SystemTestSetup(1, true);

    foo_value = ByteBuffer_MallocAndAppend(foo_value_data, sizeof(foo_value_data));
    foo_key = ByteBuffer_MallocAndAppend(foo_key_data, sizeof(foo_key_data));
    foo_tag = ByteBuffer_Malloc(20);
    foo_get_value_buf = ByteBuffer_Malloc(foo_value.bytesUsed);
    foo_get_tag_buf = ByteBuffer_Malloc(20);

    bar_value = ByteBuffer_MallocAndAppend(bar_value_data, sizeof(bar_value_data));
    bar_key = ByteBuffer_MallocAndAppend(bar_key_data, sizeof(bar_key_data));
    bar_tag = ByteBuffer_Malloc(20);
    bar_get_value_buf = ByteBuffer_Malloc(bar_value.bytesUsed);
    bar_get_tag_buf = ByteBuffer_Malloc(20);

    SHA1(foo_value.array.data, foo_value.bytesUsed, &foo_sha1[0]);
    ByteBuffer_Append(&foo_tag, foo_sha1, sizeof(foo_sha1));
    SHA1(bar_value.array.data, bar_value.bytesUsed, &bar_sha1[0]);
    ByteBuffer_Append(&bar_tag, bar_sha1, sizeof(bar_sha1));

    // Init batch operation
    batchOp = KineticClient_InitBatchOperation(Fixture.session);
}

void tearDown(void)
{
    // Free buffers
    ByteBuffer_Free(foo_value);
    ByteBuffer_Free(foo_key);
    ByteBuffer_Free(foo_tag);
    ByteBuffer_Free(foo_get_value_buf);
    ByteBuffer_Free(foo_get_tag_buf);
    ByteBuffer_Free(bar_value);
    ByteBuffer_Free(bar_key);
    ByteBuffer_Free(bar_tag);
    ByteBuffer_Free(bar_get_value_buf);
    ByteBuffer_Free(bar_get_tag_buf);

    // Free batch operation
    free(batchOp);

    SystemTestShutDown();
}

void test_batch_put_delete_should_put_delete_objects_after_batch_end(void)
{
    KineticEntry put_foo_entry = {
        .key = foo_key,
        .tag = foo_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = foo_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(Fixture.session,&put_foo_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, put_foo_entry.algorithm);

    // Delete foo entry and put bar entry in a batch
    KineticEntry delete_foo_entry = {
    	.key = foo_key,
    };

    status = KineticClient_BatchStart(batchOp);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    status = KineticClient_BatchDelete(batchOp, &delete_foo_entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    KineticEntry put_bar_entry = {
        .key = bar_key,
        .tag = bar_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = bar_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_BatchPut(batchOp, &put_bar_entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Commit the batch
    status = KineticClient_BatchEnd(batchOp);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Foo should have been deleted
    KineticEntry get_foo_entry = {
        .key = foo_key,
		.metadataOnly = true,
    };
    status = KineticClient_Get(Fixture.session,&get_foo_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    TEST_ASSERT_ByteArray_EMPTY(get_foo_entry.value.array);
    TEST_ASSERT_ByteArray_EMPTY(get_foo_entry.tag.array);

    // Bar should have been stored in the device
    KineticEntry get_bar_entry = {
        .key = bar_key,
		.value = bar_get_value_buf,
		.tag = bar_get_tag_buf,
    };
    status = KineticClient_Get(Fixture.session,&get_bar_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(get_bar_entry.tag, put_bar_entry.tag);
    TEST_ASSERT_EQUAL(get_bar_entry.algorithm, put_bar_entry.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(get_bar_entry.value, put_bar_entry.value);

    // Delete the bar entry
    KineticEntry delete_bar_entry = {
        .key = bar_key,
		.force = true,
		.synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Delete(Fixture.session,&delete_bar_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}


void test_batch_put_delete_should_not_put_delete_objects_after_batch_abort(void)
{
    KineticEntry put_foo_entry = {
        .key = foo_key,
        .tag = foo_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = foo_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(Fixture.session,&put_foo_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, put_foo_entry.algorithm);

    // Delete foo entry and put bar entry in a batch
    KineticEntry delete_foo_entry = {
    	.key = foo_key,
    };

    status = KineticClient_BatchStart(batchOp);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    status = KineticClient_BatchDelete(batchOp, &delete_foo_entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    KineticEntry put_bar_entry = {
        .key = bar_key,
        .tag = bar_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = bar_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_BatchPut(batchOp, &put_bar_entry);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Abort the batch
    status = KineticClient_BatchAbort(batchOp);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Foo should have not been deleted
    KineticEntry get_foo_entry = {
        .key = foo_key,
		.value = foo_get_value_buf,
		.tag = foo_get_tag_buf,
    };
    status = KineticClient_Get(Fixture.session,&get_foo_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(get_foo_entry.tag, foo_tag);
    TEST_ASSERT_EQUAL(get_foo_entry.algorithm, put_foo_entry.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(get_foo_entry.value, foo_value);

    // Bar should have not been stored in the device
    KineticEntry get_bar_entry = {
        .key = bar_key,
		.metadataOnly = true,
    };
    status = KineticClient_Get(Fixture.session,&get_bar_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
    TEST_ASSERT_ByteArray_EMPTY(get_bar_entry.value.array);
    TEST_ASSERT_ByteArray_EMPTY(get_bar_entry.tag.array);

    KineticEntry re_delete_foo_entry = {
        .key = foo_key,
		.force = true,
		.synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Delete(Fixture.session,&re_delete_foo_entry,NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
