#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/param.h>

#include "kinetic_client.h"

// Link dependencies, since built using Ceedling
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "byte_array.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
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
#include "zlog/zlog.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"

#include <sys/time.h>
#include <stdio.h>
 
#include <time.h>
#ifdef __MACH__ // Since time.h on OSX does not supply clock_gettime()
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#define BUFSIZE  (128 * KINETIC_OBJ_SIZE)
#define KINETIC_KEY_SIZE (1000)
#define KINETIC_MAX_THREADS (10)
#define MAX_OBJ_SIZE (KINETIC_KEY_SIZE)

KineticSessionHandle* kinetic_client;
const char HmacKeyString[] = "asdfasdf";

struct kinetic_thread_arg {
    char ip[16];
    KineticSessionHandle sessionHandle;
    char keyPrefix[KINETIC_KEY_SIZE];
    uint8_t key[KINETIC_KEY_SIZE];
    uint8_t version[KINETIC_KEY_SIZE];
    uint8_t tag[KINETIC_KEY_SIZE];
    uint8_t value[KINETIC_OBJ_SIZE];
    KineticEntry entry;
    ByteBuffer data;
    KineticStatus status;
};

void setUp()
{
    KineticClient_Init(NULL);
    // KineticClient_Init("stdout");
}

void tearDown()
{
    KineticClient_Shutdown();
}

void* kinetic_put(void* kinetic_arg)
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

        // Move dbVersion back to newVersion, since successful PUTs do this
        // in order to sync with the actual entry on disk
        if (entry->newVersion.array.data == NULL) {
            entry->newVersion = entry->dbVersion;
            entry->dbVersion = BYTE_BUFFER_NONE;
        }

        // Prepare the next chunk of data to store
        ByteBuffer_Reset(&entry->value);
        ByteBuffer_AppendArray(
            &entry->value,
            ByteBuffer_Consume(&arg->data, MIN(ByteBuffer_BytesRemaining(arg->data), MAX_OBJ_SIZE))
        );

        // Set operation-specific attributes
        // entry->synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH;

        // Store the data slice
        LOGF("  *** Storing a data slice (%u bytes)", entry->value.bytesUsed);
        KineticStatus status = KineticClient_Put(arg->sessionHandle, entry);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        LOGF("  *** KineticClient put to disk success, ip:%s", arg->ip);

        objIndex++;
    }

    gettimeofday(&stopTime, NULL);
    int64_t elapsed_us = ((stopTime.tv_sec - startTime.tv_sec) * 1000000)
        + (stopTime.tv_usec - startTime.tv_usec);
    float elapsed_ms = elapsed_us / 1000.0f;
    float throughput = (arg->data.array.len * 1000.0f) / (elapsed_ms * 1024 * 1024);
    printf("\n"
        "Write/Put Performance:\n"
        "----------------------------------------\n"
        "wrote:      %.1f kB\n"
        "duration:   %.3f seconds\n"
        "entries:    %d entries\n"
        "throughput: %.1f MB/sec\n\n",
        arg->data.array.len / 1024.0f,
        elapsed_ms / 1000.0f,
        objIndex,
        throughput);

    // Configure GetKeyRange request
    const int maxKeys = 5;
    char startKey[KINETIC_KEY_SIZE];
    ByteBuffer startKeyBuffer = ByteBuffer_Create(startKey, sizeof(startKey), 0);
    ByteBuffer_AppendCString(&startKeyBuffer, arg->keyPrefix);
    ByteBuffer_AppendCString(&startKeyBuffer, "00");
    char endKey[KINETIC_KEY_SIZE];
    ByteBuffer endKeyBuffer = ByteBuffer_Create(endKey, sizeof(endKey), 0);
    ByteBuffer_AppendCString(&endKeyBuffer, arg->keyPrefix);
    ByteBuffer_AppendCString(&endKeyBuffer, "03");
    KineticKeyRange keyRange = {
        .startKey = startKeyBuffer,
        .endKey = endKeyBuffer,
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = maxKeys,
        .reverse = false,
    };

    uint8_t keysData[maxKeys][KINETIC_MAX_KEY_LEN];
    ByteBuffer keys[maxKeys];
    for (int i = 0; i < maxKeys; i++) {
        keys[i] = ByteBuffer_Create(&keysData[i], sizeof(keysData[i]), 0);
    }

    KineticStatus status = KineticClient_GetKeyRange(arg->sessionHandle,
        &keyRange, keys, maxKeys);
    LOGF("GetKeyRange completed w/ status: %s", Kinetic_GetStatusDescription(status));
    int numKeys = 0;
    for (int i = 0; i < maxKeys; i++) {
        if (keys[i].bytesUsed > 0) {
            KineticLogger_LogByteBuffer("key", keys[i]);
            numKeys++;
        }
    }
    TEST_ASSERT_EQUAL(4, numKeys);

    return (void*)0;
}

void test_kinetic_client_should_be_able_to_store_an_arbitrarily_large_binary_object_and_split_across_entries_via_ovelapped_IO_operations(void)
{
    const int maxIterations = 2;
    const int numCopiesToStore = 3;
    const KineticSession sessionConfig = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .nonBlocking = false,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };

    for (int iteration = 0; iteration < maxIterations; iteration++) {

        printf("\n*** Overlapped PUT operation (iteration %d of %d)\n",
               iteration + 1, maxIterations);

        char* buf = malloc(sizeof(char) * BUFSIZE);
        int fd = open("test/support/data/test.data", O_RDONLY);
        int dataLen = read(fd, buf, BUFSIZE);
        close(fd);
        TEST_ASSERT_MESSAGE(dataLen > 0, "read error");

        /* thread structure */
        struct kinetic_thread_arg* kt_arg;
        pthread_t thread_id[KINETIC_MAX_THREADS];

        kinetic_client = malloc(sizeof(KineticSessionHandle) * numCopiesToStore);
        TEST_ASSERT_NOT_NULL_MESSAGE(kinetic_client, "kinetic_client malloc failed");

        kt_arg = malloc(sizeof(struct kinetic_thread_arg) * numCopiesToStore);
        TEST_ASSERT_NOT_NULL_MESSAGE(kt_arg, "kinetic_thread_arg malloc failed");

        for (int i = 0; i < numCopiesToStore; i++) {

            printf("  *** Overlapped PUT operations (writing copy %d of %d)"
                   " on IP (iteration %d of %d):%s\n",
                   i + 1, numCopiesToStore, iteration + 1,
                   maxIterations, sessionConfig.host);

            // Establish connection
            TEST_ASSERT_EQUAL_KineticStatus(
                KINETIC_STATUS_SUCCESS,
                KineticClient_Connect(&sessionConfig, &kinetic_client[i]));
            strcpy(kt_arg[i].ip, sessionConfig.host);

            // Configure the KineticEntry
            struct timeval now;
            gettimeofday(&now, NULL);
            snprintf(kt_arg[i].keyPrefix, sizeof(kt_arg[i].keyPrefix), "%010llu_%02d%02d_", (unsigned long long)now.tv_sec, iteration, i);
            ByteBuffer keyBuf = ByteBuffer_Create(kt_arg[i].key, sizeof(kt_arg[i].key), 0);
            ByteBuffer_AppendCString(&keyBuf, kt_arg[i].keyPrefix);

            ByteBuffer verBuf = ByteBuffer_Create(kt_arg[i].version, sizeof(kt_arg[i].version), 0);
            ByteBuffer_AppendCString(&verBuf, "v1.0");

            ByteBuffer tagBuf = ByteBuffer_Create(kt_arg[i].tag, sizeof(kt_arg[i].tag), 0);
            ByteBuffer_AppendCString(&tagBuf, "some_value_tag...");

            ByteBuffer valBuf = ByteBuffer_Create(kt_arg[i].value, sizeof(kt_arg[i].value), 0);

            kt_arg[i].entry = (KineticEntry) {
                .key = keyBuf,
                .newVersion = verBuf,
                .tag = tagBuf,
                .metadataOnly = false,
                .algorithm = KINETIC_ALGORITHM_SHA1,
                .value = valBuf,
            };

            // Create a ByteBuffer for consuming chunks of data out of for overlapped PUTs
            kt_arg[i].data = ByteBuffer_Create(buf, dataLen, 0);

            // Spawn the worker thread
            kt_arg[i].sessionHandle = kinetic_client[i];
            int create_status = pthread_create(&thread_id[i], NULL, kinetic_put, &kt_arg[i]);
            TEST_ASSERT_EQUAL_MESSAGE(0, create_status, "pthread create failed");
        }

        // Wait for each overlapped PUT operations to complete and cleanup
        printf("  *** Waiting for PUT threads to exit...\n");
        for (int i = 0; i < numCopiesToStore; i++) {
            int join_status = pthread_join(thread_id[i], NULL);
            TEST_ASSERT_EQUAL_MESSAGE(0, join_status, "pthread join failed");
            KineticClient_Disconnect(&kinetic_client[i]);
        }

        // Cleanup the rest of the reources
        free(kinetic_client);
        free(kt_arg);
        free(buf);

        printf("  *** Iteration complete!\n");
    }

    printf("*** Overlapped PUT operation test complete!\n");
}
