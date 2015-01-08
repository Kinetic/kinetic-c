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
#include "system_test_fixture.h"
#include "kinetic_client.h"

static SystemTestFixture Fixture;

void setUp(void)
{
    SystemTestSetup(&Fixture, 2);
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

void test_Flush_should_flush_pending_PUTs_and_DELETEs(void)
{
    LOG_LOCATION;
    LOG0("Need to figure out why this test is failing to do a basic PUT!");

    TEST_IGNORE_MESSAGE("Need to track down some odd test failures here...");

    // Arguments shared between entries
    uint8_t tagData[1024];
    uint8_t keyData[1024];
    uint8_t valueData[1024];
    
    KineticEntry Entry = {
        .key = ByteBuffer_CreateAndAppendCString(keyData, sizeof(keyData), "key1"),
        .tag = ByteBuffer_CreateAndAppendCString(tagData, sizeof(tagData), "some_tag_hash"),
        .value = ByteBuffer_CreateAndAppendCString(valueData, sizeof(valueData), "value1"),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        .force = true,
    };
    KineticStatus status = KineticClient_Put(&Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    Entry = (KineticEntry) {
        .key = ByteBuffer_CreateAndAppendCString(keyData, sizeof(keyData), "key2"),
        .tag = ByteBuffer_CreateAndAppendCString(tagData, sizeof(tagData), "some_tag_hash"),
        .value = ByteBuffer_CreateAndAppendCString(valueData, sizeof(valueData), "value1"),
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        .force = true,
    };
    status = KineticClient_Put(&Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // // Do an async DELETE so we can flush to complete it
    // KineticEntry deleteEntry = {
    //     .key = KeyBuffer1,
    // };
    // status = KineticClient_Delete(&Fixture.session, &deleteEntry, &no_op_closure);

    // /* Now do a blocking flush and confirm that (key1,value1) has been
    //  * DELETEd and (key2,value2) have been PUT. */
    // status = KineticClient_Flush(&Fixture.session, NULL);
    // TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // // GET key1 --> expect NOT FOUND
    // KineticEntry getEntry1 = {
    //     .key = KeyBuffer1,
    //     .dbVersion = VersionBuffer,
    //     .tag = TagBuffer,
    //     .algorithm = KINETIC_ALGORITHM_SHA1,
    //     .value = ValueBuffer1,
    //     .force = true,
    // };
    // status = KineticClient_Get(&Fixture.session, &getEntry1, NULL);
    // TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);

    // // GET key2 --> present
    // KineticEntry getEntry2 = {
    //     .key = KeyBuffer2,
    //     .dbVersion = VersionBuffer,
    //     .tag = TagBuffer,
    //     .algorithm = KINETIC_ALGORITHM_SHA1,
    //     .value = ValueBuffer2,
    //     .force = true,
    // };
    // status = KineticClient_Get(&Fixture.session, &getEntry2, NULL);
    // TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
