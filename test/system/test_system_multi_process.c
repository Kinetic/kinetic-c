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
#include <unistd.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>

#include "system_test_fixture.h"
#include "kinetic_client.h"
#include "kinetic_semaphore.h"

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} OpStatus;

static const int MaxDaemons = 2;
static int NumDaemons = 0;
static int CurrentPort;

static void op_finished(KineticCompletionData* kinetic_data, void* clientData);

static void child_task(void) {

    srand(time(NULL) + getpid()); // re-randomize this process

    const size_t num_ops = 100;
    const size_t obj_size = KINETIC_OBJ_SIZE;

    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&config);

    KineticStatus status;
    ByteArray hmacArray = ByteArray_CreateWithCString("asdfasdf");
    KineticSessionConfig sessionConfig = {
        .host = SYSTEM_TEST_HOST,
        .port = CurrentPort,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = hmacArray,
    };
    KineticSession* session;
    status = KineticClient_CreateSession(&sessionConfig, client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    sleep(2); // Delay to allow other processes to become connected to maximize overlapping

    pid_t pid = getpid();
    LOGF0("Starting I/O in process %lu", (unsigned long)pid);

    // Generate test entry data
    ByteBuffer test_data = ByteBuffer_Malloc(obj_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);
    uint8_t tag_data[] = {0x00, 0x01, 0x02, 0x03};
    ByteBuffer tag = ByteBuffer_Create(tag_data, sizeof(tag_data), sizeof(tag_data));
    uint64_t keys[num_ops];
    KineticEntry entries[num_ops];
    uint64_t r = rand();
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

        size_t bytes_written = 0;

        for (uint32_t put = 0; put < num_ops; put++) {
            ByteBuffer key = ByteBuffer_Create(&keys[put], sizeof(keys[put]), sizeof(keys[put]));

            KineticSynchronization sync = KINETIC_SYNCHRONIZATION_WRITEBACK;
            if ((put == num_ops - 1) || (num_ops % 7 == 0)) {
                sync = KINETIC_SYNCHRONIZATION_FLUSH;
            }

            ByteBuffer my_data = test_data;
            // my_data.array.len = rand() % KINETIC_OBJ_SIZE;
            // my_data.bytesUsed = my_data.array.len;

            entries[put] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .algorithm = KINETIC_ALGORITHM_SHA1,
                .value = my_data,
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
                sprintf(msg, "PUT(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }

            bytes_written += my_data.bytesUsed;
        }

        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(put_statuses[i].sem);
            if (put_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "PUT(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(put_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_written * 1000.0f) / (elapsed_ms * 1024 * 1024);
        LOGF0("PUT(pid=%lu) Performance: wrote %.1f kB, duration: %.2f sec, throughput: %.2f MB/sec",
            (unsigned long)pid,
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
        for (size_t i = 0; i < num_ops; i++) {
            test_get_datas[i] = ByteBuffer_Malloc(obj_size);
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
                session,
                &entries[get],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &get_statuses[get],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "GET(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        size_t bytes_read = 0;
        for (size_t i = 0; i < num_ops; i++)
        {
            KineticSemaphore_WaitForSignalAndDestroy(get_statuses[i].sem);
            if (get_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "GET(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(get_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
            else {
                bytes_read += entries[i].value.bytesUsed;
            }
        }

        // Check data for integrity
        size_t numFailures = 0;
        for (size_t i = 0; i < num_ops; i++) {
            int res = memcmp(test_data.array.data, test_get_datas[i].array.data, test_get_datas[i].bytesUsed);
            if (res != 0) {
                LOGF0("Failed validating data in object %zu of %zu (pid=%lu)!", i+1, num_ops);
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
        LOGF0("GET(pid=%lu) Performance: wrote %.1f kB, duration: %.2f sec, throughput: %.2f MB/sec",
            (unsigned long)pid,
            bytes_read / 1024.0f,
            elapsed_ms / 1000.0f,
            bandwidth);

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
                session,
                &entries[del],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &delete_statuses[del],
                }
            );

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "DELETE(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        for (size_t i = 0; i < num_ops; i++) {
            KineticSemaphore_WaitForSignalAndDestroy(delete_statuses[i].sem);
            if (delete_statuses[i].status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "DELETE(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(delete_statuses[i].status));
                TEST_FAIL_MESSAGE(msg);
            }
        }

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);
        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float throughput = (num_ops * 1000.0f) / elapsed_ms;
        LOGF0("DELETE(pid=%lu) Performance: count %zu entries, duration %.3f sec, throughput: %.2f entries/sec",
            (unsigned long)pid,
            num_ops,
            elapsed_ms / 1000.0f,
            throughput);
    }

    // Shutdown client connection and cleanup
    ByteBuffer_Free(test_data);
    status = KineticClient_DestroySession(session);
    TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when disconnecting client!");
    KineticClient_Shutdown(client);
    exit(0);
}

static void op_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    OpStatus * op_status = clientData;
    // Save operation result status
    op_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(op_status->sem);
}

void test_kinetic_client_stress_multiple_processes(void)
{
    srand(time(NULL));
    CurrentPort = KINETIC_PORT;
    for (NumDaemons = 0; NumDaemons < MaxDaemons; NumDaemons++) {
        pid_t pid = fork();
        if (pid == 0) {
            LOGF0("\nStarting kinetic daemon on port %d", CurrentPort);
            child_task();
        } else if (pid == -1) {
            err(1, "fork");
        } else {
            CurrentPort++;
        }
    }

    for (int i = 0; i < MaxDaemons; i++) {
        int stat_loc = 0;
        wait(&stat_loc);
    }
}
