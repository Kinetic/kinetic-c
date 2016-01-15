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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#define MAX_ITERATIONS (1)
#define NUM_COPIES (3)
#define MAX_OBJ_SIZE (KINETIC_OBJ_SIZE)

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

STATIC const char HmacKeyString[] = "asdfasdf";
STATIC const int TestDataSize = 200 * KINETIC_OBJ_SIZE;

struct kinetic_thread_arg {
    char ip[16];
    KineticSession* session;
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

KineticClient * client;
void setUp(void)
{
    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 0,
    };
    client = KineticClient_Init(&config);
}

void tearDown(void)
{
    if (client) {
        KineticClient_Shutdown(client);
    }
}

void test_kinetic_client_should_be_able_to_store_an_arbitrarily_large_binary_object_and_split_across_entries_via_overlapped_IO_operations(void)
{
    KineticSessionConfig sessionConfig = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(sessionConfig.host, GetSystemTestHost1(), sizeof(sessionConfig.host)-1);
    sessionConfig.port = GetSystemTestPort1();

    float bandwidthAccumulator = 0.0f, minBandwidth = 1000000000.0f, maxBandwidth = -1000000000.0f;
    float aggregateBandwidthPerIteration[MAX_ITERATIONS];

    for (int iteration = 0; iteration < MAX_ITERATIONS; iteration++) {

        printf("\n*** Overlapped PUT operation (iteration %d of %d)\n",
               iteration + 1, MAX_ITERATIONS);
        TEST_ASSERT_MESSAGE(TestDataSize > 0, "read error");

        // Allocate session/thread data
        struct kinetic_thread_arg* kt_arg;
        pthread_t thread_id[NUM_COPIES];
        kt_arg = malloc(sizeof(struct kinetic_thread_arg) * NUM_COPIES);
        TEST_ASSERT_NOT_NULL_MESSAGE(kt_arg, "kinetic_thread_arg malloc failed");
        uint8_t* testData = malloc(TestDataSize);
        ByteBuffer testBuf = ByteBuffer_CreateAndAppendDummyData(testData, TestDataSize, TestDataSize);
        ByteBuffer_Reset(&testBuf);

        // Establish all of the connection first, so their session can all get initialized first
        for (int i = 0; i < NUM_COPIES; i++) {
            // Establish connection
            TEST_ASSERT_EQUAL_KineticStatus(
                KINETIC_STATUS_SUCCESS,
                KineticClient_CreateSession(&sessionConfig, client, &kt_arg[i].session));
            strcpy(kt_arg[i].ip, sessionConfig.host);
            kt_arg[i].data = testBuf;

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
            KineticClient_DestroySession(kt_arg[i].session);

            // Update results for summary
            bandwidthAccumulator += kt_arg[i].bandwidth;
            aggregateBandwidthPerIteration[iteration] += kt_arg[i].bandwidth;
            minBandwidth = MIN(kt_arg[i].bandwidth, minBandwidth);
            maxBandwidth = MAX(kt_arg[i].bandwidth, maxBandwidth);
        }

        // Cleanup the rest of the reources
        free(kt_arg);
        free(testData);

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
            ByteBuffer_Consume(&arg->data, MAX_OBJ_SIZE)
        );

        // Set operation-specific attributes
        if (ByteBuffer_BytesRemaining(arg->data) == 0) {
            entry->synchronization = KINETIC_SYNCHRONIZATION_FLUSH;
        }
        else {
            entry->synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK;
        }

        // Store the data slice
        LOGF1("  *** Storing a data slice (%zu bytes)", entry->value.bytesUsed);
        KineticStatus status = KineticClient_Put(arg->session, entry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        LOGF1("  *** KineticClient put to disk success, ip:%s", arg->ip);

        objIndex++;
    }

    gettimeofday(&stopTime, NULL);


    int64_t elapsed_us = ((stopTime.tv_sec - startTime.tv_sec) * 1000000)
        + (stopTime.tv_usec - startTime.tv_usec);
        LOGF0("elapsed us = %lu", elapsed_us);
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
