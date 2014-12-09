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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#define MAX_ITERATIONS (2)
#define NUM_COPIES (3)
#define BUFSIZE  (128 * KINETIC_OBJ_SIZE)
#define KINETIC_MAX_THREADS (10)
#define MAX_OBJ_SIZE (KINETIC_OBJ_SIZE)

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

STATIC const char HmacKeyString[] = "asdfasdf";
STATIC int SourceDataSize;

struct kinetic_thread_arg {
    char ip[16];
    KineticSession session;
    char keyPrefix[KINETIC_DEFAULT_KEY_LEN];
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t version[KINETIC_DEFAULT_KEY_LEN];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    uint8_t value[KINETIC_OBJ_SIZE];
    KineticEntry entry;
    ByteBuffer data;
    KineticStatus status;
    float bandwidth;
};

static void* kinetic_put(void* kinetic_arg);


void setUp(void)
{
    KineticClient_Init("stdout", 0);
}

void tearDown(void)
{
    KineticClient_Shutdown();
}

void test_kinetic_client_should_be_able_to_store_an_arbitrarily_large_binary_object_and_split_across_entries_via_ovelapped_IO_operations(void)
{
    const KineticSessionConfig sessionConfig = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };

    float bandwidthAccumulator = 0.0f, minBandwidth = 1000000000.0f, maxBandwidth = -1000000000.0f;
    float aggregateBandwidthPerIteration[MAX_ITERATIONS];

    for (int iteration = 0; iteration < MAX_ITERATIONS; iteration++) {

        printf("\n*** Overlapped PUT operation (iteration %d of %d)\n",
               iteration + 1, MAX_ITERATIONS);

        char* buf = malloc(sizeof(char) * BUFSIZE);
        int fd = open("test/support/data/test.data", O_RDONLY);
        SourceDataSize = read(fd, buf, BUFSIZE);
        close(fd);
        TEST_ASSERT_MESSAGE(SourceDataSize > 0, "read error");

        // Allocate session/thread data
        struct kinetic_thread_arg* kt_arg;
        pthread_t thread_id[KINETIC_MAX_THREADS];
        kt_arg = malloc(sizeof(struct kinetic_thread_arg) * NUM_COPIES);
        TEST_ASSERT_NOT_NULL_MESSAGE(kt_arg, "kinetic_thread_arg malloc failed");

        // Establish all of the connection first, so their session can all get initialized first
        for (int i = 0; i < NUM_COPIES; i++) {
            // Establish connection
            kt_arg[i].session.config = sessionConfig;
            TEST_ASSERT_EQUAL_KineticStatus(
                KINETIC_STATUS_SUCCESS,
                KineticClient_CreateConnection(&kt_arg[i].session));
            strcpy(kt_arg[i].ip, sessionConfig.host);

            // Create a ByteBuffer for consuming chunks of data out of for overlapped PUTs
            kt_arg[i].data = ByteBuffer_Create(buf, SourceDataSize, 0);

            // Configure the KineticEntry
            struct timeval now;
            gettimeofday(&now, NULL);
            snprintf(kt_arg[i].keyPrefix, sizeof(kt_arg[i].keyPrefix), "%010llu_%02d%02d_",
                (unsigned long long)now.tv_sec, iteration, i);
            ByteBuffer keyBuf = ByteBuffer_Create(kt_arg[i].key, sizeof(kt_arg[i].key), 0);
            ByteBuffer_AppendCString(&keyBuf, kt_arg[i].keyPrefix);
            ByteBuffer verBuf = ByteBuffer_Create(kt_arg[i].version, sizeof(kt_arg[i].version), 0);
            ByteBuffer_AppendCString(&verBuf, "v1.0");
            ByteBuffer tagBuf = ByteBuffer_Create(kt_arg[i].tag, sizeof(kt_arg[i].tag), 0);
            ByteBuffer_AppendCString(&tagBuf, "some_value_tag...");
            ByteBuffer valBuf = ByteBuffer_Create(kt_arg[i].value, sizeof(kt_arg[i].value), 0);
            kt_arg[i].entry = (KineticEntry) {
                .key = keyBuf,
                // .newVersion = verBuf,
                .tag = tagBuf,
                .algorithm = KINETIC_ALGORITHM_SHA1,
                .value = valBuf,
            };
        }

        // Write all of the copies simultaneously (overlapped)
        for (int i = 0; i < NUM_COPIES; i++) {
            printf("  *** Overlapped PUT operations (writing copy %d of %d)"
                   " on IP (iteration %d of %d):%s\n",
                   i + 1, NUM_COPIES, iteration + 1,
                   MAX_ITERATIONS, sessionConfig.host);

            // Spawn the thread
            int pthreadStatus = pthread_create(&thread_id[i], NULL, kinetic_put, &kt_arg[i]);
            REPORT_ERRNO(pthreadStatus, "pthread_create");
            TEST_ASSERT_EQUAL_MESSAGE(0, pthreadStatus, "pthread create failed");
        }

        // Wait for each overlapped PUT operations to complete and cleanup
        printf("  *** Waiting for PUT threads to exit...\n");
        aggregateBandwidthPerIteration[iteration] = 0.0f; 
        for (int i = 0; i < NUM_COPIES; i++) {
            int join_status = pthread_join(thread_id[i], NULL);
            TEST_ASSERT_EQUAL_MESSAGE(0, join_status, "pthread join failed");
            KineticClient_DestroyConnection(&kt_arg[i].session);

            // Update results for summary
            bandwidthAccumulator += kt_arg[i].bandwidth;
            aggregateBandwidthPerIteration[iteration] += kt_arg[i].bandwidth;
            minBandwidth = MIN(kt_arg[i].bandwidth, minBandwidth);
            maxBandwidth = MAX(kt_arg[i].bandwidth, maxBandwidth);
        }

        // Cleanup the rest of the reources
        free(kt_arg);
        free(buf);

        fflush(stdout);
        printf("  *** Iteration complete!\n");
        fflush(stdout);
    }

    fflush(stdout);
    printf("\n*** Overlapped PUT operation test complete!\n\n");
    double meanBandwidth = bandwidthAccumulator / (MAX_ITERATIONS * NUM_COPIES);
    double meanAggregateBandwidth = 0.0f;
    for (int iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        meanAggregateBandwidth += aggregateBandwidthPerIteration[iteration];
    }
    meanAggregateBandwidth /= MAX_ITERATIONS;
    printf("========================================\n");
    printf("=         Performance Summary          =\n");
    printf("========================================\n");
    printf("Min write bandwidth:      %.2f (MB/sec)\n", minBandwidth);
    printf("Max write bandwidth:      %.2f (MB/sec)\n", maxBandwidth);
    printf("Mean write bandwidth:     %.2f (MB/sec)\n", meanBandwidth);
    printf("Mean aggregate bandwidth: %.2f (MB/sec)\n", meanAggregateBandwidth);
    printf("\n");
    fflush(stdout);
}


static void* kinetic_put(void* kinetic_arg)
{
    struct kinetic_thread_arg* arg = kinetic_arg;
    KineticEntry* entry = &(arg->entry);
    int32_t objIndex = 0;
    struct timeval startTime, stopTime;
    
    gettimeofday(&startTime, NULL);

    while (ByteBuffer_BytesRemaining(arg->data) > 0) {

        // Configure meta-data
        char keySuffix[8];
        snprintf(keySuffix, sizeof(keySuffix), "%02d", objIndex);
        entry->key.bytesUsed = strlen(arg->keyPrefix);
        ByteBuffer_AppendCString(&entry->key, keySuffix);

        // Prepare the next chunk of data to store
        ByteBuffer_Reset(&entry->value);
        ByteBuffer_AppendArray(
            &entry->value,
            ByteBuffer_Consume(&arg->data, MIN(ByteBuffer_BytesRemaining(arg->data), MAX_OBJ_SIZE))
        );

        // Set operation-specific attributes
        entry->synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK;

        // Store the data slice
        LOGF1("  *** Storing a data slice (%zu bytes)", entry->value.bytesUsed);
        KineticStatus status = KineticClient_Put(&arg->session, entry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        LOGF1("  *** KineticClient put to disk success, ip:%s", arg->ip);

        objIndex++;
    }

    gettimeofday(&stopTime, NULL);

    int64_t elapsed_us = ((stopTime.tv_sec - startTime.tv_sec) * 1000000)
        + (stopTime.tv_usec - startTime.tv_usec);
    float elapsed_ms = elapsed_us / 1000.0f;
    arg->bandwidth = (arg->data.array.len * 1000.0f) / (elapsed_ms * 1024 * 1024);
    fflush(stdout);
    printf("\n"
        "Write/Put Performance:\n"
        "----------------------------------------\n"
        "wrote:      %.1f kB\n"
        "duration:   %.3f seconds\n"
        "entries:    %d entries\n"
        "throughput: %.2f MB/sec\n\n",
        arg->data.array.len / 1024.0f,
        elapsed_ms / 1000.0f,
        objIndex,
        arg->bandwidth);
    fflush(stdout);

    return (void*)0;
}
