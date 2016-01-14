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

struct key_struct {
    char data[32];
};

static const int MaxDaemons = 2;
static int NumDaemons = 0;
static char* CurrentHost;
static int CurrentPort;

static char* Hosts[2];
static int Ports[2];

static void op_finished(KineticCompletionData* kinetic_data, void* clientData);

static void child_task(void) {

    srand(time(NULL) + getpid()); // re-randomize this process

    const size_t num_ops = 200;
    const size_t obj_size = KINETIC_OBJ_SIZE;

    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&config);

    KineticStatus status;
    KineticSession* session;
    KineticSessionConfig sessionConfig = {
        .port = CurrentPort,
        .clusterVersion = SESSION_CLUSTER_VERSION,
        .identity = SESSION_IDENTITY,
    };
    const char* hmacKey = "asdfasdf";
    strncpy((char*)sessionConfig.keyData, hmacKey, strlen(hmacKey));
    sessionConfig.hmacKey = ByteArray_Create(sessionConfig.keyData, strlen(hmacKey));
    strncpy((char*)sessionConfig.host, CurrentHost, sizeof(sessionConfig.host)-1);
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

    KineticEntry* entries = calloc(num_ops, sizeof(KineticEntry));
    KineticCompletionClosure* closures = calloc(num_ops, sizeof(KineticCompletionClosure));
    struct key_struct* keyValues = calloc(num_ops, sizeof(struct key_struct));
    ByteBuffer *keys = calloc(num_ops, sizeof(ByteBuffer));
    OpStatus put_statuses[num_ops];

    for (uint32_t i = 0; i < num_ops; i++) {
        keys[i] = ByteBuffer_CreateAndAppendFormattedCString(
            &keyValues[i].data, sizeof(struct key_struct), "%08u", (unsigned int)i);

        KineticSynchronization sync = (i == num_ops - 1)
            ? KINETIC_SYNCHRONIZATION_FLUSH
            : KINETIC_SYNCHRONIZATION_WRITEBACK;

        entries[i] = (KineticEntry) {
            .key = keys[i],
            .tag = tag,
            .algorithm = KINETIC_ALGORITHM_SHA3,
            .value = test_data,
            .synchronization = sync,
            .force = true,
        };

        put_statuses[i] = (OpStatus){
            .sem = KineticSemaphore_Create(),
            .status = KINETIC_STATUS_INVALID,
        };

        closures[i] = (KineticCompletionClosure) {
            .callback = op_finished,
            .clientData = &put_statuses[i],
        };
    }

    // Measure PUT performance
    {
        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        size_t bytes_written = 0;

        for (uint32_t i = 0; i < num_ops; i++) {
            KineticStatus status = KineticClient_Put(
                session, &entries[i], &closures[i]);

            if (status != KINETIC_STATUS_SUCCESS) {
                char msg[128];
                sprintf(msg, "PUT(pid=%lu) failed w/status: %s",
                    (unsigned long)pid, Kinetic_GetStatusDescription(status));
                TEST_FAIL_MESSAGE(msg);
            }

            bytes_written += test_data.bytesUsed;
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

        for (uint32_t i = 0; i < num_ops; i++) {
            entries[i] = (KineticEntry) {
                .key = keys[i],
                .tag = tag,
                .value = test_get_datas[i],
            };

            KineticStatus status = KineticClient_Get(
                session,
                &entries[i],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &get_statuses[i],
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

        for (uint32_t i = 0; i < num_ops; i++) {

            KineticStatus status = KineticClient_Delete(
                session,
                &entries[i],
                &(KineticCompletionClosure) {
                    .callback = op_finished,
                    .clientData = &delete_statuses[i],
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
    free(keys);
    free(keyValues);
    free(entries);
    free(closures);
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
    Hosts[0] = GetSystemTestHost1();
    Ports[0] = GetSystemTestPort1();
    Hosts[1] = GetSystemTestHost2();
    Ports[1] = GetSystemTestPort2();
    for (NumDaemons = 0; NumDaemons < MaxDaemons; NumDaemons++) {
        pid_t pid = fork();
        if (pid == 0) {
            CurrentHost = Hosts[NumDaemons];
            CurrentPort = Ports[NumDaemons];
            LOGF0("\nStarting kinetic daemon on port %d", CurrentPort);
            child_task();
        } else if (pid == -1) {
            err(1, "fork");
        } else {
            // CurrentPort++;
        }
    }

    for (int i = 0; i < MaxDaemons; i++) {
        int stat_loc = 0;
        wait(&stat_loc);
        if (WIFEXITED(stat_loc)) {
            uint8_t exit_status = WEXITSTATUS(stat_loc);
            char err[64];
            sprintf(err, "Forked process %d of %d exited with error!", i+1, MaxDaemons);
            TEST_ASSERT_EQUAL_MESSAGE(0u, exit_status, err);
        }
    }
}
