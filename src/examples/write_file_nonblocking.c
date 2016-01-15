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

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

struct kinetic_thread_arg {
    char ip[16];
    struct kinetic_put_arg* opArgs;
    int opCount;
};

typedef struct {
    size_t opsInProgress;
    size_t currentChunk;
    size_t maxOverlappedChunks;
    int fd;
    uint64_t keyPrefix;
    pthread_mutex_t transferMutex;
    pthread_mutex_t completeMutex;
    pthread_cond_t completeCond;
    KineticStatus status;
    KineticSession* session;
} FileTransferProgress;

typedef struct {
    KineticEntry entry;
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t value[KINETIC_OBJ_SIZE];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    FileTransferProgress* currentTransfer;
} AsyncWriteClosureData;

FileTransferProgress * start_file_transfer(KineticSession* session,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks);
KineticStatus wait_for_put_finish(FileTransferProgress* const transfer);

static int put_chunk_of_file(FileTransferProgress* transfer);
static void put_chunk_of_file_finished(KineticCompletionData* kinetic_data, void* client_data);


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Initialize kinetic-c and establish session
    KineticSession* session;
    KineticClientConfig client_config = {
        .logFile = "stdout",
        .logLevel = 0,
    };
    KineticClient * client = KineticClient_Init(&client_config);
    if (client == NULL) { return 1; }
    const char HmacKeyString[] = "asdfasdf";
    KineticSessionConfig config = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticStatus status = KineticClient_CreateSession(&config, client, &session);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        return -1;
    }

    // Create a unique/common key prefix
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t prefix = (uint64_t)now.tv_sec << sizeof(8);

    // Kick off the chained write/PUT operations and wait for completion
    const uint32_t maxOverlappedChunks = 4;
    const char* dataFile = "test/support/data/test.data";
    FileTransferProgress* transfer = start_file_transfer(session, dataFile, prefix, maxOverlappedChunks);
    printf("Waiting for transfer to complete...\n");
    status = wait_for_put_finish(transfer);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Transfer failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        return -2;
    }
    printf("Transfer completed successfully!\n");

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);

    return 0;
}

static int put_chunk_of_file(FileTransferProgress* transfer)
{
    AsyncWriteClosureData* closureData = calloc(1, sizeof(AsyncWriteClosureData));
    transfer->opsInProgress++;
    closureData->currentTransfer = transfer;

    int bytesRead = read(transfer->fd, closureData->value, sizeof(closureData->value));
    if (bytesRead > 0) {
        transfer->currentChunk++;
        closureData->entry = (KineticEntry){
            .key = ByteBuffer_CreateAndAppend(closureData->key, sizeof(closureData->key),
                &transfer->keyPrefix, sizeof(transfer->keyPrefix)),
            .tag = ByteBuffer_CreateAndAppendFormattedCString(closureData->tag, sizeof(closureData->tag),
                "some_value_tag..._%04d", transfer->currentChunk),
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = ByteBuffer_Create(closureData->value, sizeof(closureData->value), (size_t)bytesRead),
            .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };
        KineticStatus status = KineticClient_Put(transfer->session,
            &closureData->entry,
            &(KineticCompletionClosure) {
                .callback = put_chunk_of_file_finished,
                .clientData = closureData,
            });
        if (status != KINETIC_STATUS_SUCCESS) {
            transfer->opsInProgress--;
            free(closureData);
            fprintf(stderr, "Failed writing chunk! PUT request reported status: %s\n",
                Kinetic_GetStatusDescription(status));
        }
    }
    else if (bytesRead == 0) { // EOF reached
        transfer->opsInProgress--;
        free(closureData);
    }
    else {
        transfer->opsInProgress--;
        free(closureData);
        fprintf(stderr, "Failed reading data from file!\n");
        REPORT_ERRNO(bytesRead, "read");
    }

    return bytesRead;
}

static void put_chunk_of_file_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    AsyncWriteClosureData* closureData = clientData;
    FileTransferProgress* currentTransfer = closureData->currentTransfer;
    free(closureData);
    currentTransfer->opsInProgress--;

    if (kinetic_data->status == KINETIC_STATUS_SUCCESS) {
        int bytesPut = put_chunk_of_file(currentTransfer);
        if (bytesPut <= 0 && currentTransfer->opsInProgress == 0) {
            if (currentTransfer->status == KINETIC_STATUS_NOT_ATTEMPTED) {
                currentTransfer->status = KINETIC_STATUS_SUCCESS;
            }
            pthread_cond_signal(&currentTransfer->completeCond);
        }
    }
    else {
        currentTransfer->status = kinetic_data->status;
        // only signal when finished
        // keep track of outstanding operations
        // if there is no more data to read (or error), and no outstanding operations,
        // then signal
        pthread_cond_signal(&currentTransfer->completeCond);
        fprintf(stderr, "Failed writing chunk! PUT response reported status: %s\n",
            Kinetic_GetStatusDescription(kinetic_data->status));
    }
}

FileTransferProgress * start_file_transfer(KineticSession* session,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks)
{
    FileTransferProgress * transferState = malloc(sizeof(FileTransferProgress));
    *transferState = (FileTransferProgress) {
        .session = session,
        .maxOverlappedChunks = maxOverlappedChunks,
        .keyPrefix = keyPrefix,
        .fd = open(filename, O_RDONLY),
    };
    pthread_mutex_init(&transferState->transferMutex, NULL);
    pthread_mutex_init(&transferState->completeMutex, NULL);
    pthread_cond_init(&transferState->completeCond, NULL);

    // Start max overlapped PUT operations
    for (size_t i = 0; i < transferState->maxOverlappedChunks; i++) {
        put_chunk_of_file(transferState);
    }
    return transferState;
}

KineticStatus wait_for_put_finish(FileTransferProgress* const transfer)
{
    pthread_mutex_lock(&transfer->completeMutex);
    pthread_cond_wait(&transfer->completeCond, &transfer->completeMutex);
    pthread_mutex_unlock(&transfer->completeMutex);

    KineticStatus status = transfer->status;

    pthread_mutex_destroy(&transfer->completeMutex);
    pthread_cond_destroy(&transfer->completeCond);

    close(transfer->fd);

    free(transfer);

    return status;
}
