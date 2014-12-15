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
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_device_info.h"
#include "kinetic_serial_allocator.h"
#include "kinetic_proto.h"
#include "kinetic_allocator.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

static SystemTestFixture Fixture;

void setUp(void)
{
    SystemTestSetup(&Fixture);
}

void tearDown(void)
{
    SystemTestTearDown(&Fixture);
}

void test_Flush_should_succeed(void)
{
    KineticStatus status = KineticClient_Flush(&Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_Flush_should_be_idempotent(void)
{
    KineticStatus status = KineticClient_Flush(&Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    status = KineticClient_Flush(&Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

typedef struct {
    bool flag;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} completion_test_env;

static void completion_cb(KineticCompletionData* kinetic_data, void* client_data)
{
    TEST_ASSERT_NOT_NULL(client_data);
    completion_test_env *env = (completion_test_env *)client_data;
    env->flag = true;
    TEST_ASSERT_EQUAL(0, pthread_cond_signal(&env->cond));
    (void)kinetic_data;
}

void test_Flush_should_call_callback_after_completion(void)
{
    completion_test_env env;
    memset(&env, 0, sizeof(env));

    TEST_ASSERT_EQUAL(0, pthread_cond_init(&env.cond, NULL));
    TEST_ASSERT_EQUAL(0, pthread_mutex_init(&env.mutex, NULL));

    KineticCompletionClosure closure = {
        .callback = completion_cb,
        .clientData = (void *)&env
    };

    KineticStatus status = KineticClient_Flush(&Fixture.session, &closure);

    /* Wait up to 10 seconds for the callback to fire. */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct timespec ts = {ts.tv_sec = tv.tv_sec + 10};
    int res = pthread_cond_timedwait(&env.cond, &env.mutex, &ts);
    TEST_ASSERT_EQUAL(0, res);

    TEST_ASSERT_TRUE(env.flag);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, pthread_cond_destroy(&env.cond));
}

static void no_op_callback(KineticCompletionData* kinetic_data, void* client_data)
{
    (void)kinetic_data;
    (void)client_data;
}

void test_Flush_should_flush_pending_async_PUTs_and_DELETEs(void)
{
    if (SystemTestIsUnderSimulator()) {
        TEST_IGNORE_MESSAGE("FLUSHALLDATA is a no-op in the simulator.");
    }

    // Arguments shared between entries
    uint8_t VersionData[1024];
    ByteBuffer VersionBuffer = ByteBuffer_Create(VersionData, sizeof(VersionData), 0);
    uint8_t TagData[1024];
    ByteBuffer TagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "some_tag_hash");

    uint8_t key1[] = "key1";
    uint8_t value1[] = "value1";
    ByteBuffer KeyBuffer1 = ByteBuffer_Create(key1, sizeof(key1), 0);
    ByteBuffer ValueBuffer1 = ByteBuffer_Create(value1, sizeof(value1), 0);

    uint8_t key2[] = "key2";
    uint8_t value2[] = "value2";
    ByteBuffer KeyBuffer2 = ByteBuffer_Create(key2, sizeof(key2), 0);
    ByteBuffer ValueBuffer2 = ByteBuffer_Create(value2, sizeof(value2), 0);

    // Do a blocking PUT ("key1" => "value1") so we can delete it later
    KineticEntry Entry = (KineticEntry) {
        .key = KeyBuffer1,
        .newVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer1,
        .force = true,
    };
    KineticStatus status = KineticClient_Put(&Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Do an async PUT ("key2" => "value2") so we can flush to complete it
    Entry = (KineticEntry) {
        .key = KeyBuffer2,
        .newVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer2,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        .force = true,
    };
    KineticCompletionClosure no_op_closure = {
        .callback = &no_op_callback,
    };

    // Include a closure to signal that the PUT should be non-blocking.
    status = KineticClient_Put(&Fixture.session, &Entry, &no_op_closure);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Do an async DELETE so we can flush to complete it
    KineticEntry deleteEntry = {
        .key = KeyBuffer1,
    };
    status = KineticClient_Delete(&Fixture.session, &deleteEntry, &no_op_closure);

    /* Now do a blocking flush and confirm that (key1,value1) has been
     * DELETEd and (key2,value2) have been PUT. */
    status = KineticClient_Flush(&Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Reset buffers for GET requests
    ByteBuffer_Reset(&ValueBuffer1);
    ByteBuffer_Reset(&ValueBuffer2);

    // GET key1 --> expect NOT FOUND
    KineticEntry getEntry1 = {
        .key = KeyBuffer1,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer1,
        .force = true,
    };
    status = KineticClient_Get(&Fixture.session, &getEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);

    // GET key2 --> present
    KineticEntry getEntry2 = {
        .key = KeyBuffer2,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer2,
        .force = true,
    };
    status = KineticClient_Get(&Fixture.session, &getEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}



/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
