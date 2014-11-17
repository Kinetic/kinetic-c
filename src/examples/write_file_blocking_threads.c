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

#define MAX_ITERATIONS (2)
#define NUM_COPIES (3)
#define BUFSIZE  (128 * KINETIC_OBJ_SIZE)
#define KINETIC_MAX_THREADS (10)
#define MAX_OBJ_SIZE (KINETIC_OBJ_SIZE)

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

typedef struct {
    pthread_t threadID;
    char ip[16];
    KineticSessionHandle sessionHandle;
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
    int32_t objIndex = 0;

    while (ByteBuffer_BytesRemaining(thread_args->data) > 0) {

        // Configure meta-data
        char keySuffix[8];
        snprintf(keySuffix, sizeof(keySuffix), "%02d", objIndex);
        entry->key.bytesUsed = strlen(thread_args->keyPrefix);
        ByteBuffer_AppendCString(&entry->key, keySuffix);

        // Prepare the next chunk of data to store
        ByteBuffer_Reset(&entry->value);
        ByteBuffer_AppendArray(
            &entry->value,
            ByteBuffer_Consume(&thread_args->data, MIN(ByteBuffer_BytesRemaining(thread_args->data), MAX_OBJ_SIZE))
        );

        // Set operation-specific attributes
        entry->synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK;

        // Store the data slice
        KineticStatus status = KineticClient_Put(thread_args->sessionHandle, entry, NULL);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Failed writing entry %d to disk w/status: %s",
                objIndex+1, Kinetic_GetStatusDescription(status));
            return (void*)NULL;
        }

        objIndex++;
    }

    printf("File stored to successfully to Kinetic Device across %d entries!\n", objIndex);

    return (void*)NULL;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Read in file contents to store
    KineticStatus status;
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

    // Allocate session/thread data
    write_args* writeArgs = calloc(NUM_COPIES, sizeof(write_args));
    if (writeArgs == NULL) {
        fprintf(stderr, "Failed allocating overlapped thread arguments!\n");
    }

    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    const KineticSession sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .nonBlocking = false,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticClient_Init("stdout", 0);

    // Establish all of the connection first, so their session can all get initialized first
    for (int i = 0; i < NUM_COPIES; i++) {

        // Establish connection
        status = KineticClient_Connect(&sessionConfig, &writeArgs[i].sessionHandle);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
                Kinetic_GetStatusDescription(status));
            return -1;
        }
        strcpy(writeArgs[i].ip, sessionConfig.host);

        // Create a ByteBuffer for consuming chunks of data out of for overlapped PUTs
        writeArgs[i].data = ByteBuffer_Create(buf, dataLen, 0);

        // Configure the KineticEntry
        struct timeval now;
        gettimeofday(&now, NULL);
        snprintf(writeArgs[i].keyPrefix, sizeof(writeArgs[i].keyPrefix), "%010llu_%02d_",
            (unsigned long long)now.tv_sec, i);
        ByteBuffer keyBuf = ByteBuffer_Create(writeArgs[i].key, sizeof(writeArgs[i].key), 0);
        ByteBuffer_AppendCString(&keyBuf, writeArgs[i].keyPrefix);
        ByteBuffer verBuf = ByteBuffer_Create(writeArgs[i].version, sizeof(writeArgs[i].version), 0);
        ByteBuffer_AppendCString(&verBuf, "v1.0");
        ByteBuffer tagBuf = ByteBuffer_Create(writeArgs[i].tag, sizeof(writeArgs[i].tag), 0);
        ByteBuffer_AppendCString(&tagBuf, "some_value_tag...");
        ByteBuffer valBuf = ByteBuffer_Create(writeArgs[i].value, sizeof(writeArgs[i].value), 0);
        writeArgs[i].entry = (KineticEntry) {
            .key = keyBuf,
            // .newVersion = verBuf,
            .tag = tagBuf,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = valBuf,
        };
    }
    sleep(2); // Give a generous chunk of time for session to be initialized by the target device

    // Write all of the copies simultaneously (overlapped)
    for (int i = 0; i < NUM_COPIES; i++) {
        printf("  *** Overlapped PUT operations (writing copy %d of %d)"
               " on IP: %s\n", i + 1, NUM_COPIES, sessionConfig.host);
        int threadCreateStatus = pthread_create(&writeArgs[i].threadID, NULL, store_data, &writeArgs[i]);
        REPORT_ERRNO(threadCreateStatus, "pthread_create");
        if (threadCreateStatus != 0) {
            fprintf(stderr, "pthread create failed!\n");
            exit(-2);
        }
    }

    // Wait for each overlapped PUT operations to complete and cleanup
    printf("  *** Waiting for PUT threads to exit...\n");
    for (int i = 0; i < NUM_COPIES; i++) {
        int joinStatus = pthread_join(writeArgs[i].threadID, NULL);
        if (joinStatus != 0) {
            fprintf(stderr, "pthread join failed!\n");
        }
        KineticClient_Disconnect(&writeArgs[i].sessionHandle);
    }

    // Shutdown client connection and cleanup
    KineticClient_Shutdown();
    free(writeArgs);
    free(buf);

    return 0;
}
