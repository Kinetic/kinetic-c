#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include "kinetic_client.h"

// Link dependencies, since built using Ceedling
#include "unity.h"
#include "unity_helper.h"
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
#include "protobuf-c/protobuf-c.h"
#include "socket99/socket99.h"

#define KINETIC_OBJ_SIZE (PDU_VALUE_MAX_LEN)
#define BUFSIZE  (128 * KINETIC_OBJ_SIZE)
#define KINETIC_KEY_SIZE (1000)
#define KINETIC_MAX_THREADS (10)

KineticSessionHandle* kinetic_client;

void setUp(void)
{
}

void tearDown(void)
{
}

struct kinetic_thread_arg {
    char ip[16];
    KineticSessionHandle sessionHandle;
    KineticKeyValue metaData;
    uint8_t oid[KINETIC_KEY_SIZE];
    ByteBuffer data;
    KineticStatus status;
};

void* kinetic_put(void* kinetic_arg)
{
    struct kinetic_thread_arg* arg = kinetic_arg;
    KineticKeyValue* metaData = &(arg->metaData);
    int32_t objIndex = 0;
    uint8_t key[KINETIC_KEY_SIZE];

    while (arg->data.bytesUsed < arg->data.array.len) {

        // Configure meta-data
        memset(key, 0, sizeof(key));
        sprintf((char*)key, "%s_%02d", arg->oid, objIndex);
        metaData->key = (ByteArray) {
            .data = key, .len = sizeof(key)
        };
        int bytesRemaining = arg->data.array.len - arg->data.bytesUsed;
        metaData->value = (ByteArray) {
            .data = &(arg->data.array.data[objIndex * KINETIC_OBJ_SIZE]),
             .len = (bytesRemaining > KINETIC_OBJ_SIZE) ? KINETIC_OBJ_SIZE : bytesRemaining,
        };
        arg->data.bytesUsed += metaData->value.len;

        // Store the data slice
        LOGF("Storing a data slice (%u bytes)", metaData->value.len);
        KineticStatus status = KineticClient_Put(arg->sessionHandle, metaData);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        LOGF("KineticClient put to disk success, ip:%s", arg->ip);

        objIndex++;
    }

    return (void*)0;
}

void test_kinetic_client_should_be_able_to_store_an_arbitrarily_large_binary_object_and_split_across_entries_via_ovelapped_IO_operations(void)
{
    const int maxIterations = 4;
    const int numCopiesToStore = 4;
    const char* keyPrefix = "some_prefix";
    const KineticSession sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .nonBlocking = false,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString("asdfasdf"),
        .logFile = "NONE",
    };

    for (int iteration = 0; iteration < maxIterations; iteration++) {

        printf("Overlapped PUT operation (iteration %d of %d)\n",
               iteration + 1, maxIterations);

        int fd, dataLen, i;

        char* buf = malloc(sizeof(char) * BUFSIZE);
        fd = open("test/support/data/test.data", O_RDONLY);
        dataLen = read(fd, buf, BUFSIZE);
        close(fd);
        TEST_ASSERT_MESSAGE(dataLen > 0, "read error");

        /* thread structure */
        KineticKeyValue metaDataDefaults = {
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .newVersion = ByteArray_CreateWithCString("v1.0"),
            .dbVersion = BYTE_ARRAY_NONE,
            .tag = ByteArray_CreateWithCString("some_value_tag..."),
            .metadataOnly = false,
        };
        struct kinetic_thread_arg* kt_arg;
        pthread_t thread_id[KINETIC_MAX_THREADS];

        kinetic_client = malloc(sizeof(KineticSessionHandle) * numCopiesToStore);
        TEST_ASSERT_NOT_NULL_MESSAGE(kinetic_client, "kinetic_client malloc failed");

        kt_arg = malloc(sizeof(struct kinetic_thread_arg) * numCopiesToStore);
        TEST_ASSERT_NOT_NULL_MESSAGE(kt_arg, "kinetic_thread_arg malloc failed");

        for (i = 0; i < numCopiesToStore; i++) {

            printf("    Overlapped PUT operations (writing copy %d of %d) on IP:%s\n",
                   i + 1, numCopiesToStore, sessionConfig.host);

            // Establish connection
            TEST_ASSERT_EQUAL_KineticStatus(
                KINETIC_STATUS_SUCCESS,
                KineticClient_Connect(&sessionConfig, &kinetic_client[i]));

            // Configure the key and object data
            strcpy(kt_arg[i].ip, sessionConfig.host);
            sprintf((char*)kt_arg[i].oid, "%s%02d" "_%02d", keyPrefix, iteration, i);
            kt_arg[i].metaData = metaDataDefaults;
            ByteArray dataArray = {.data = (uint8_t*)buf, .len = dataLen};
            kt_arg[i].data = BYTE_BUFFER_INIT(dataArray);

            // Spawn the worker thread
            kt_arg[i].sessionHandle = kinetic_client[i];
            int err_ret = pthread_create(&thread_id[i], NULL, kinetic_put, &kt_arg[i]);
            TEST_ASSERT_EQUAL_MESSAGE(0, err_ret, "pthread create failed");
        }

        // Wait for each overlapped PUT operations to complete and cleanup
        printf("  Waiting for PUT threads to exit...\n");
        for (i = 0; i < numCopiesToStore; i++) {
            int err_ret = pthread_join(thread_id[i], NULL);
            TEST_ASSERT_EQUAL_MESSAGE(0, err_ret, "pthread join failed");
            KineticClient_Disconnect(&kinetic_client[i]);
        }

        // Cleanup the rest of the reources
        free(kinetic_client);
        free(kt_arg);
        free(buf);

        printf("  Iteration complete!\n");
    }

    printf("Overlapped PUT operation test complete!\n");
}
