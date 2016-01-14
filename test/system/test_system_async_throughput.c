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

void setUp(void)
{
    SystemTestSetup(1, true);
}

void tearDown(void)
{
    SystemTestShutDown();
}

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} OpStatus;

static void run_throghput_tests(size_t num_ops, size_t value_size);

void test_kinetic_client_throughput_for_maximum_sized_objects(void)
{
    run_throghput_tests(500, KINETIC_OBJ_SIZE);
}

void test_kinetic_client_throughput_for_small_sized_objects(void)
{
    run_throghput_tests(200, 120);
}

static void op_finished(KineticCompletionData* kinetic_data, void* clientData);

struct key_struct {
    uint8_t data[32];
};

static void run_throghput_tests(size_t num_ops, size_t value_size)
{
    printf("\n"
        "========================================\n"
        "Throughput Test\n"
        "========================================\n"
        "Entry Size: %zu bytes\n"
        "Count:      %zu entries\n\n",
        value_size, num_ops);

    ByteBuffer test_data = ByteBuffer_Malloc(value_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    uint8_t tag_data[] = {0x00, 0x01, 0x02, 0x03};
    ByteBuffer tag = ByteBuffer_Create(tag_data, sizeof(tag_data), sizeof(tag_data));

    struct key_struct* keys = calloc(num_ops, sizeof(struct key_struct));
    KineticEntry* entries = calloc(num_ops, sizeof(KineticEntry));
    OpStatus* op_statuses = calloc(num_ops, sizeof(OpStatus));
    KineticCompletionClosure* closures = calloc(num_ops, sizeof(KineticCompletionClosure));
    ByteBuffer* test_get_datas = calloc(num_ops, sizeof(ByteBuffer));

    // Measure PUT performance
    {
        for (size_t i = 0; i < num_ops; i++) {
            op_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
        };

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (size_t i = 0; i < num_ops; i++) {
            ByteBuffer key = ByteBuffer_CreateAndAppendFormattedCString(&keys[i], sizeof(struct key_struct), "%08zu", i);

            KineticSynchronization sync = (i == num_ops - 1)
                ? KINETIC_SYNCHRONIZATION_FLUSH
                : KINETIC_SYNCHRONIZATION_WRITEBACK;

            entries[i] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .algorithm = KINETIC_ALGORITHM_SHA3,
                .value = test_data,
                .synchronization = sync,
                .force = true,
            };

            closures[i] = (KineticCompletionClosure) {
                .callback = op_finished,
                .clientData = &op_statuses[i],
            };

            KineticStatus status = KineticClient_Put(Fixture.session, &entries[i], &closures[i]);

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        size_t num_failures = 0;
        for (size_t i = 0; i < num_ops; i++) {
            KineticSemaphore_WaitForSignalAndDestroy(op_statuses[i].sem);
            if (op_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "PUT %zu of %zu failed w/status: %s\n",
                    i+1, num_ops, Kinetic_GetStatusDescription(op_statuses[i].status));
                num_failures++;
            }
        }
        TEST_ASSERT_EQUAL_MESSAGE(0, num_failures, "PUT failures detected!");

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        size_t bytes_written = num_ops * test_data.array.len;
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_written * 1000.0f) / (elapsed_ms * 1024 * 1024);
        float entries_per_sec = num_ops * 1000.0f / elapsed_ms;
        fflush(stdout);
        printf("\n"
            "Write/Put Performance:\n"
            "----------------------------------------\n"
            "wrote:      %.1f kB\n"
            "entries:    %zu\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f MB/sec (%.1f entries/sec)\n\n",
            bytes_written / 1024.0f,
            num_ops,
            elapsed_ms / 1000.0f,
            bandwidth,
            entries_per_sec);
    }

    // Measure GET performance
    {
        for (size_t i = 0; i < num_ops; i++) {
            op_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };

            test_get_datas[i] = ByteBuffer_Malloc(value_size);

            ByteBuffer key = ByteBuffer_CreateAndAppendFormattedCString(&keys[i],
                sizeof(struct key_struct), "%08zu", i);

            entries[i] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .value = test_get_datas[i],
            };

            closures[i] = (KineticCompletionClosure) {
                .callback = op_finished,
                .clientData = &op_statuses[i],
            };
        };

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (size_t i = 0; i < num_ops; i++) {
            KineticStatus status = KineticClient_Get(Fixture.session, &entries[i], &closures[i]);

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        size_t bytes_read = 0;
        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(op_statuses[i].sem);
            if (op_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(op_statuses[i].status));
                TEST_FAIL();
            }
            else {
                bytes_read += entries[i].value.bytesUsed;
            }
        }
        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        for (size_t i = 0; i < num_ops; i++) {
            ByteBuffer_Free(test_get_datas[i]);
        }

        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_read * 1000.0f) / (elapsed_ms * 1024 * 1024);
        float entries_per_sec = num_ops * 1000.0f / elapsed_ms;
        fflush(stdout);
        printf("\n"
            "Read/Get Performance:\n"
            "----------------------------------------\n"
            "read:       %.1f kB\n"
            "entries:    %zu\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f MB/sec (%.1f entries/sec)\n\n",
            bytes_read / 1024.0f,
            num_ops,
            elapsed_ms / 1000.0f,
            bandwidth,
            entries_per_sec);
    }

    // Measure DELETE performance
    {
        for (size_t i = 0; i < num_ops; i++) {
            op_statuses[i] = (OpStatus){
                .sem = KineticSemaphore_Create(),
                .status = KINETIC_STATUS_INVALID,
            };
        };

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        for (size_t i = 0; i < num_ops; i++) {
            ByteBuffer key = ByteBuffer_CreateAndAppendFormattedCString(&keys[i], sizeof(struct key_struct), "%08u", i);

            KineticSynchronization sync = (i == num_ops - 1)
                ? KINETIC_SYNCHRONIZATION_FLUSH
                : KINETIC_SYNCHRONIZATION_WRITEBACK;

            entries[i] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .synchronization = sync,
                .force = true,
            };

            closures[i] = (KineticCompletionClosure) {
                .callback = op_finished,
                .clientData = &op_statuses[i],
            };

            KineticStatus status = KineticClient_Delete(Fixture.session, &entries[i], &closures[i]);

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "DELETE failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(op_statuses[i].sem);
            if (op_statuses[i].status != KINETIC_STATUS_SUCCESS) {

                fprintf(stderr, "DELETE failed w/status: %s\n", Kinetic_GetStatusDescription(op_statuses[i].status));
                TEST_FAIL();
            }
        }

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float throughput = (num_ops * 1000.0f) / elapsed_ms;
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
    free(keys);
    free(entries);
    free(op_statuses);
    free(closures);
    free(test_get_datas);
}

static void op_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    OpStatus * op_status = clientData;
    // Save operation result status
    op_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(op_status->sem);
}

