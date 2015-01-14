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

void run_throghput_tests(KineticClient * client, size_t num_ops, size_t value_size)
{
    printf("\n"
        "========================================\n"
        "Throughput Tests\n"
        "========================================\n"
        "Entry Size: %zu bytes\n"
        "Count:      %zu entries",
        value_size, num_ops );

    ByteBuffer test_data = ByteBuffer_Malloc(value_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = SYSTEM_TEST_HOST,
            .port = KINETIC_PORT,
            .clusterVersion = 0,
            .identity = 1,
            .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
        },
    };
    // Establish connection
    KineticStatus status = KineticClient_CreateConnection(&session, client);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }

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

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (uint32_t put = 0; put < num_ops; put++) {
            ByteBuffer key = ByteBuffer_Create(&keys[put], sizeof(keys[put]), sizeof(keys[put]));

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
                &session,
                &entries[put],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &put_statuses[put],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        printf("Waiting for PUTs to finish\n");

        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(put_statuses[i].sem);
            if (put_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(put_statuses[i].status));
                TEST_FAIL();
            }
        }

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        size_t bytes_written = num_ops * test_data.array.len;
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_written * 1000.0f) / (elapsed_ms * 1024 * 1024);
        fflush(stdout);
        printf("\n"
            "Write/Put Performance:\n"
            "----------------------------------------\n"
            "wrote:      %.1f kB\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f MB/sec\n\n",
            bytes_written / 1024.0f,
            elapsed_ms / 1000.0f,
            bandwidth);


    }

    // Measure GET performance
    {
        OpStatus get_statuses[num_ops];
        for (size_t i = 0; i < num_ops; i++) {
            get_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
        };

        ByteBuffer test_get_datas[num_ops];
        for (size_t i = 0; i < num_ops; i++)
        {
            test_get_datas[i] = ByteBuffer_Malloc(value_size);
        }

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (uint32_t get = 0; get < num_ops; get++) {
            ByteBuffer key = ByteBuffer_Create(&keys[get], sizeof(keys[get]), sizeof(keys[get]));

            entries[get] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .value = test_get_datas[get],
            };

            KineticStatus status = KineticClient_Get(
                &session,
                &entries[get],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &get_statuses[get],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        printf("Waiting for GETs to finish\n");

        size_t bytes_read = 0;
        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(get_statuses[i].sem);
            if (get_statuses[i].status != KINETIC_STATUS_SUCCESS) {

                fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(get_statuses[i].status));
                TEST_FAIL();
            }
            else
            {
                bytes_read += entries[i].value.bytesUsed;
            }
        }
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_read * 1000.0f) / (elapsed_ms * 1024 * 1024);
        fflush(stdout);
        printf("\n"
            "Read/Get Performance:\n"
            "----------------------------------------\n"
            "read:      %.1f kB\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f MB/sec\n\n",
            bytes_read / 1024.0f,
            elapsed_ms / 1000.0f,
            bandwidth);

        for (size_t i = 0; i < num_ops; i++)
        {
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

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (uint32_t del = 0; del < num_ops; del++) {
            ByteBuffer key = ByteBuffer_Create(&keys[del], sizeof(keys[del]), sizeof(keys[del]));

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
                &session,
                &entries[del],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &delete_statuses[del],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "DELETE failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        printf("Waiting for DELETEs to finish\n");

        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(delete_statuses[i].sem);
            if (delete_statuses[i].status != KINETIC_STATUS_SUCCESS) {

                fprintf(stderr, "DELETE failed w/status: %s\n", Kinetic_GetStatusDescription(delete_statuses[i].status));
                TEST_FAIL();
            }
        }
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float throughput = (num_ops * 1000.0f) / elapsed_ms;
        fflush(stdout);
        printf("\n"
            "Delete Performance:\n"
            "----------------------------------------\n"
            "count:      %zu entries\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f entries/sec\n\n",
            num_ops,
            elapsed_ms / 1000.0f,
            throughput);
    }

    ByteBuffer_Free(test_data);

    // Shutdown client connection and cleanup
    KineticClient_DestroyConnection(&session);
}


typedef struct {
    KineticClient * client;
    uint32_t num_ops;
    uint32_t obj_size;
    uint32_t thread_iters;
} TestParams;

static void* test_thread(void* test_params)
{
    TestParams * params = test_params;
    for (uint32_t i = 0; i < params->thread_iters; i++)
    {
        run_throghput_tests(params->client, params->num_ops, params->obj_size);
    }
    return NULL;
}

void run_tests(KineticClient * client)
{
    TestParams params[] = { { .client = client, .num_ops = 100, .obj_size = KINETIC_OBJ_SIZE, .thread_iters = 2 }
                          , { .client = client, .num_ops = 1000, .obj_size = 120, .thread_iters = 2 }
                          , { .client = client, .num_ops = 500, .obj_size = 70000, .thread_iters = 2 } };
                          // , { .client = client, .num_ops = 1000, .obj_size = 120, .thread_iters = 2 }
                          // , { .client = client, .num_ops = 100, .obj_size = KINETIC_OBJ_SIZE, .thread_iters = 2 } };
    pthread_t thread_id[NUM_ELEMENTS(params)];

    for (uint32_t i = 0; i < NUM_ELEMENTS(params); i ++)
    {
        int pthreadStatus = pthread_create(&thread_id[i], NULL, test_thread, &params[i]);
        TEST_ASSERT_EQUAL_MESSAGE(0, pthreadStatus, "pthread create failed");
    }

    for (uint32_t i = 0; i < NUM_ELEMENTS(params); i++) {
        int join_status = pthread_join(thread_id[i], NULL);
        TEST_ASSERT_EQUAL_MESSAGE(0, join_status, "pthread join failed");
    }
}


void test_kinetic_client_throughput_for_small_sized_objects(void)
{
    srand(time(NULL));
    for (uint32_t i = 0; i < 2; i++) {
        KineticClient * client = KineticClient_Init("stdout", 1);
        run_tests(client);
        KineticClient_Shutdown(client);
    }
}

static void op_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    OpStatus * op_status = clientData;
    // Save operation result status
    op_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(op_status->sem);
}

