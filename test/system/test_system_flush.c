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

void setUp(void)
{
    SystemTestSetup(1, true);
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_Flush_should_succeed(void)
{
    KineticStatus status = KineticClient_Flush(Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}

void test_Flush_should_be_idempotent(void)
{
    KineticStatus status = KineticClient_Flush(Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    status = KineticClient_Flush(Fixture.session, NULL);
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

    KineticStatus status = KineticClient_Flush(Fixture.session, &closure);

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
    // Arguments shared between entries
    uint8_t TagData[1024];
    ByteBuffer tagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), "tag_val");
    uint8_t key1[10];
    ByteBuffer keyBuffer1 = ByteBuffer_CreateAndAppendCString(key1, sizeof(key1), "key1");
    uint8_t value1[10];
    ByteBuffer valueBuffer1 = ByteBuffer_CreateAndAppendCString(value1, sizeof(value1), "value1");
    uint8_t key2[10];
    ByteBuffer keyBuffer2 = ByteBuffer_CreateAndAppendCString(key2, sizeof(key2), "key2");
    uint8_t value2[10];
    ByteBuffer valueBuffer2 = ByteBuffer_CreateAndAppendCString(value2, sizeof(value2), "value2");

    // Do a blocking PUT ("key1" => "value1") so we can delete it later
    KineticEntry Entry = (KineticEntry) {
        .key = keyBuffer1,
        .tag = tagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = valueBuffer1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        .force = true,
    };
    KineticStatus status = KineticClient_Put(Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    Entry = (KineticEntry) {
        .key = keyBuffer2,
        .tag = tagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = valueBuffer2,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        .force = true,
    };

    status = KineticClient_Put(Fixture.session, &Entry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Do an async DELETE so we can flush to complete it
    KineticEntry deleteEntry = {
        .key = keyBuffer1,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    KineticCompletionClosure no_op_closure = {
        .callback = &no_op_callback,
    };
    status = KineticClient_Delete(Fixture.session, &deleteEntry, &no_op_closure);

    /* Now do a blocking flush and confirm that (key1,value1) has been
     * DELETEd and (key2,value2) have been PUT. */
    status = KineticClient_Flush(Fixture.session, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // GET key1 --> expect NOT FOUND
    KineticEntry getEntry1 = {
        .key = keyBuffer1,
        .tag = tagBuffer,
        .value = valueBuffer1,
    };
    status = KineticClient_Get(Fixture.session, &getEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);

    // GET key2 --> present
    KineticEntry getEntry2 = {
        .key = keyBuffer2,
        .tag = tagBuffer,
        .value = valueBuffer2,
    };
    status = KineticClient_Get(Fixture.session, &getEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
