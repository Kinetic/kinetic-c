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
#include "kinetic_semaphore.h"
#include <stdlib.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} OpStatus;

static void op_finished(KineticCompletionData* kinetic_data, void* clientData);

void run_throughput_tests(KineticSession* session, size_t num_ops, size_t value_size)
{
    LOGF0("STRESS THREAD: object_size: %zu bytes, count: %zu entries", value_size, num_ops);

    ByteBuffer test_data = ByteBuffer_Malloc(value_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    uint8_t tag_data[] = {0x00, 0x01, 0x02, 0x03};
    ByteBuffer tag = ByteBuffer_Create(tag_data, sizeof(tag_data), sizeof(tag_data));

    uint64_t r = rand();

    uint64_t keys[num_ops];
    KineticEntry entries[num_ops];

    for (uint32_t put = 0; put < num_ops; put++) {
        keys[put] = put | (r << 16);
    }

    // Measure PUT performance
    {
        OpStatus put_statuses[num_ops];
        for (size_t i = 0; i < num_ops; i++) {
            put_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
        };

        // Record start time
        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        // Kick off a batch of async PUTs
        for (uint32_t put = 0; put < num_ops; put++) {
            ByteBuffer key = ByteBuffer_Create(&keys[put], sizeof(keys[put]), sizeof(keys[put]));

            // Only set FLUSH on last object to allow delayed persistence w/ batch FLUSH once complete
            KineticSynchronization sync = (put == num_ops - 1)
                ? KINETIC_SYNCHRONIZATION_FLUSH
                : KINETIC_SYNCHRONIZATION_WRITEBACK;

            entries[put] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .algorithm = KINETIC_ALGORITHM_SHA1,
                .value = test_data,
                .synchronization = sync,
            };

            KineticStatus status = KineticClient_Put(
                session,
                &entries[put],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &put_statuses[put],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "PUT failed w/status: %s", Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        for (size_t i = 0; i < num_ops; i++) {
            KineticSemaphore_WaitForSignalAndDestroy(put_statuses[i].sem);
            if (put_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "PUT failed w/status: %s", Kinetic_GetStatusDescription(put_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        // Calculate and report performance
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        size_t bytes_written = num_ops * test_data.array.len;
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_written * 1000.0f) / (elapsed_ms * 1024 * 1024);
        fflush(stdout);
        LOGF0("PUT Performance: wrote: %.1f kB, duration: %.3f sec, throughput: %.2f MB/sec",
            bytes_written / 1024.0f, elapsed_ms / 1000.0f, bandwidth);
    }

    // Measure GET performance
    {
        OpStatus get_statuses[num_ops];
        ByteBuffer test_get_datas[num_ops];
        for (size_t i = 0; i < num_ops; i++) {
            get_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
            test_get_datas[i] = ByteBuffer_Malloc(value_size);
        };

        // Record start time
        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        // Kick off a batch of async GETss
        for (uint32_t get = 0; get < num_ops; get++) {
            ByteBuffer key = ByteBuffer_Create(&keys[get], sizeof(keys[get]), sizeof(keys[get]));

            entries[get] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .value = test_get_datas[get],
            };

            KineticStatus status = KineticClient_Get(
                session,
                &entries[get],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &get_statuses[get],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "GET failed w/status: %s", Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        size_t bytes_read = 0;
        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(get_statuses[i].sem);
            if (get_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "GET failed w/status: %s", Kinetic_GetStatusDescription(get_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
            else
            {
                bytes_read += entries[i].value.bytesUsed;
            }
        }

        // Check data for integrity
        size_t numFailures = 0;
        for (size_t i = 0; i < num_ops; i++) {
            int res = memcmp(test_data.array.data, test_get_datas[i].array.data, test_data.array.len);
            if (res != 0) {
                LOGF0("Failed validating data in object %zu of %zu!", i+1, num_ops);
                numFailures++;
            }
        }
        TEST_ASSERT_EQUAL_MESSAGE(0, numFailures, "DATA INTEGRITY CHECK FAILED UPON READBACK!");

        // Calculate and report performance
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_read * 1000.0f) / (elapsed_ms * 1024 * 1024);
        fflush(stdout);
        LOGF0("GET Performance: read: %.1f kB, duration: %.3f sec, throughput: %.2f MB/sec",
            bytes_read / 1024.0f, elapsed_ms / 1000.0f, bandwidth);
        for (size_t i = 0; i < num_ops; i++) {
            ByteBuffer_Free(test_get_datas[i]);
        }
    }

    // Measure DELETE performance
    {
        OpStatus delete_statuses[num_ops];
        for (size_t i = 0; i < num_ops; i++) {
            delete_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
        };

        // Record start time
        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        // Kick off a batch of async DELETEs
        for (uint32_t del = 0; del < num_ops; del++) {
            ByteBuffer key = ByteBuffer_Create(&keys[del], sizeof(keys[del]), sizeof(keys[del]));

            // Only set FLUSH on last object to allow delayed persistence w/ batch FLUSH once complete
            KineticSynchronization sync = (del == num_ops - 1)
                ? KINETIC_SYNCHRONIZATION_FLUSH
                : KINETIC_SYNCHRONIZATION_WRITEBACK;

            entries[del] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .synchronization = sync,
                .force = true,
            };

            KineticStatus status = KineticClient_Delete(
                session,
                &entries[del],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &delete_statuses[del],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "DELETE failed w/status: %s", Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        for (size_t i = 0; i < num_ops; i++) {
            KineticSemaphore_WaitForSignalAndDestroy(delete_statuses[i].sem);
            if (delete_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "DELETE failed w/status: %s", Kinetic_GetStatusDescription(delete_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        // Calculate and report performance
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float throughput = (num_ops * 1000.0f) / elapsed_ms;
        fflush(stdout);
        LOGF0("DELETE Performance: count: %zu entries, duration: %.3f sec, throughput: %.2f entries/sec",
            num_ops, elapsed_ms / 1000.0f, throughput);
    }

    ByteBuffer_Free(test_data);
}


typedef struct {
    KineticClient * client;
    KineticSession * session;
    uint32_t num_ops;
    uint32_t obj_size;
    uint32_t thread_iters;
} TestParams;

static void* test_thread(void* test_params)
{
    TestParams * params = test_params;
    for (uint32_t i = 0; i < params->thread_iters; i++) {
        run_throughput_tests(params->session, params->num_ops, params->obj_size);
    }
    return NULL;
}

void run_tests(KineticClient * client)
{
    // Configure and establish a session with the specified device
    KineticSession* session;
    const char HmacKeyString[] = "asdfasdf";
    KineticSessionConfig config = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(config.host, GetSystemTestHost1(), sizeof(config.host)-1);
    config.port = GetSystemTestPort1();
    KineticStatus status = KineticClient_CreateSession(&config, client, &session);
    if (status != KINETIC_STATUS_SUCCESS) {
        char msg[128];
        sprintf(msg, "Failed connecting to the Kinetic device w/status: %s", Kinetic_GetStatusDescription(status));
        TEST_FAIL_MESSAGE(msg);
    }

    // Prepare per-thread test data
    TestParams params[] = {
        { .client = client, .session = session, .thread_iters = 1, .num_ops = 250,  .obj_size = KINETIC_OBJ_SIZE },
        { .client = client, .session = session, .thread_iters = 1, .num_ops = 500,  .obj_size = 120,             },
        { .client = client, .session = session, .thread_iters = 1, .num_ops = 750,  .obj_size = 500,             },
        { .client = client, .session = session, .thread_iters = 1, .num_ops = 250,  .obj_size = 70000,           },
        // { .client = client, .session = session, .thread_iters = 2, .num_ops = 1000, .obj_size = 120,             },
        // { .client = client, .session = session, .thread_iters = 3, .num_ops = 1000, .obj_size = 120,             },
        // { .client = client, .session = session, .thread_iters = 2, .num_ops = 100,  .obj_size = KINETIC_OBJ_SIZE },
        // { .client = client, .session = session, .thread_iters = 5, .num_ops = 1000, .obj_size = 120,             },
        // { .client = client, .session = session, .thread_iters = 2, .num_ops = 100,  .obj_size = KINETIC_OBJ_SIZE },
    };

    pthread_t thread_id[NUM_ELEMENTS(params)];

    for (uint32_t i = 0; i < NUM_ELEMENTS(params); i++) {
        int pthreadStatus = pthread_create(&thread_id[i], NULL, test_thread, &params[i]);
        TEST_ASSERT_EQUAL_MESSAGE(0, pthreadStatus, "pthread create failed");
    }

    for (uint32_t i = 0; i < NUM_ELEMENTS(params); i++) {
        int join_status = pthread_join(thread_id[i], NULL);
        TEST_ASSERT_EQUAL_MESSAGE(0, join_status, "pthread join failed");
    }

    // Shutdown client session and cleanup
    KineticClient_DestroySession(session);
}

void test_kinetic_client_throughput_test_kinetic_client_throughput_(void)
{
    srand(time(NULL));
    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 0,
        .readerThreads = 1,
        .maxThreadpoolThreads = 1,
    };
    KineticClient * client = KineticClient_Init(&config);

    const uint32_t max_runs = 1;
    for (uint32_t i = 0; i < max_runs; i++) {
        LOGF0(
            "============================================================================================\n"
            "==  Test run %u of %u\n"
            "============================================================================================\n",
            i+1, max_runs);
        run_tests(client);
    }

    KineticClient_Shutdown(client);

}

static void op_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    OpStatus * op_status = clientData;
    // Save operation result status
    op_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(op_status->sem);
}
