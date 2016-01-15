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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "byte_array.h"
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>
#include <errno.h>

#define NUM_FILES (3)

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

typedef struct {
    pthread_t threadID;
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
} write_args;

void* store_data(void* args)
{
    write_args* thread_args = (write_args*)args;
    KineticEntry* entry = &(thread_args->entry);
    int32_t objIndex;
    for (objIndex = 0; ByteBuffer_BytesRemaining(thread_args->data) > 0; objIndex++) {

        // Configure meta-data
        char keySuffix[8];
        snprintf(keySuffix, sizeof(keySuffix), "%02d", objIndex);
        entry->key.bytesUsed = strlen(thread_args->keyPrefix);
        ByteBuffer_AppendCString(&entry->key, keySuffix);
        entry->synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK;

        // Prepare the next chunk of data to store
        ByteBuffer_AppendArray(&entry->value, ByteBuffer_Consume(&thread_args->data, KINETIC_OBJ_SIZE));

        // Store the data slice
        KineticStatus status = KineticClient_Put(thread_args->session, entry, NULL);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Failed writing entry %d to disk w/status: %s",
                objIndex+1, Kinetic_GetStatusDescription(status));
            return (void*)NULL;
        }
    }
    printf("File stored successfully to Kinetic device across %d entries!\n", objIndex);
    return (void*)NULL;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Initialize kinetic-c and configure sessions
    KineticSession* session;
    KineticClientConfig clientConfig = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&clientConfig);
    if (client == NULL) { return 1; }
    const char HmacKeyString[] = "asdfasdf";
    KineticSessionConfig sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticStatus status = KineticClient_CreateSession(&sessionConfig, client, &session);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        return -1;
    }

    // Read in file contents to store
    const char* dataFile = "test/support/data/test.data";
    struct stat st;
    stat(dataFile, &st);
    char* buf = malloc(st.st_size);
    int fd = open(dataFile, O_RDONLY);
    long dataLen = read(fd, buf, st.st_size);
    close(fd);
    if (dataLen <= 0) {
        fprintf(stderr, "Failed reading data file to store: %s\n", dataFile);
        exit(-1);
    }

    write_args* writeArgs = calloc(NUM_FILES, sizeof(write_args));
    if (writeArgs == NULL) {
        fprintf(stderr, "Failed allocating overlapped thread arguments!\n");
    }

    // Kick off a thread for each file to store
    for (int i = 0; i < NUM_FILES; i++) {

        // Establish connection
        status = KineticClient_CreateSession(&sessionConfig, client, &writeArgs[i].session);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
                Kinetic_GetStatusDescription(status));
            return -1;
        }
        strcpy(writeArgs[i].ip, sessionConfig.host);

        // Create a ByteBuffer for consuming chunks of data out of for overlapped PUTs
        writeArgs[i].data = ByteBuffer_Create(buf, dataLen, 0);

        // Configure common entry attributes
        struct timeval now;
        gettimeofday(&now, NULL);
        snprintf(writeArgs[i].keyPrefix, sizeof(writeArgs[i].keyPrefix), "%010llu_%02d_",
            (unsigned long long)now.tv_sec, i);
        ByteBuffer valBuf = ByteBuffer_Create(writeArgs[i].value, sizeof(writeArgs[i].value), 0);
        writeArgs[i].entry = (KineticEntry) {
            .key = ByteBuffer_CreateAndAppendCString(
                writeArgs[i].key, sizeof(writeArgs[i].key), writeArgs[i].keyPrefix),
            .tag = ByteBuffer_CreateAndAppendCString(
                writeArgs[i].tag, sizeof(writeArgs[i].tag), "some_value_tag..."),
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = valBuf,
        };

        // Store the entry
        int threadCreateStatus = pthread_create(&writeArgs[i].threadID, NULL, store_data, &writeArgs[i]);
        REPORT_ERRNO(threadCreateStatus, "pthread_create");
        if (threadCreateStatus != 0) {
            fprintf(stderr, "pthread create failed!\n");
            exit(-2);
        }
    }

    // Wait for all PUT operations to complete and cleanup
    for (int i = 0; i < NUM_FILES; i++) {
        int joinStatus = pthread_join(writeArgs[i].threadID, NULL);
        if (joinStatus != 0) {
            fprintf(stderr, "pthread join failed!\n");
        }
        KineticClient_DestroySession(writeArgs[i].session);
    }

    // Shutdown client connection and cleanup
    KineticClient_Shutdown(client);
    free(writeArgs);
    free(buf);

    return 0;
}
