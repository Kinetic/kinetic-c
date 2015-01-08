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
#include "system_test_fixture.h"
#include "kinetic_client.h"
#include <stdlib.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>

STATIC const int TestDataSize = 100 * (1024*1024);

struct kinetic_thread_arg {
    char ip[16];
    struct kinetic_put_arg* opArgs;
    int opCount;
};

typedef struct {
    size_t opsInProgress;
    size_t currentChunk;
    size_t maxOverlappedChunks;
    FILE* fp;
    uint64_t keyPrefix;
    pthread_mutex_t transferMutex;
    pthread_mutex_t completeMutex;
    pthread_cond_t completeCond;
    KineticStatus status;
    KineticSession* session;
    size_t bytesWritten;
    struct timeval startTime;
} FileTransferProgress;

typedef struct {
    KineticEntry entry;
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t value[KINETIC_OBJ_SIZE];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    FileTransferProgress* currentTransfer;
} AsyncWriteClosureData;

FileTransferProgress * start_file_transfer(KineticSession * const session,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks);
KineticStatus wait_for_put_finish(FileTransferProgress* const transfer);

static int put_chunk_of_file(FileTransferProgress* transfer);
static void put_chunk_of_file_finished(KineticCompletionData* kinetic_data, void* client_data);

void test_kinetic_client_should_store_a_binary_object_split_across_entries_via_ovelapped_asynchronous_IO_operations(void)
{
    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = "localhost",
            .port = KINETIC_PORT,
            .clusterVersion = 0,
            .identity = 1,
            .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
        },
    };
    KineticClient * client = KineticClient_Init("stdout", 0);

    // Establish connection
    KineticStatus status = KineticClient_CreateConnection(&session, client);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }

    // Create a unique/common key prefix
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t prefix = (uint64_t)now.tv_sec << sizeof(8);

    // Kick off the chained write/PUT operations and wait for completion
    const char* dataFile = "test/support/data/test.data";
    FileTransferProgress* transfer = start_file_transfer(&session, dataFile, prefix, 4);
    if (transfer != NULL) {
        printf("Waiting for transfer to complete...\n");
        status = wait_for_put_finish(transfer);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Transfer failed w/status: %s\n",
                Kinetic_GetStatusDescription(status));
            TEST_FAIL();
        }
        printf("Transfer completed successfully!\n");
    }
    else {
        printf("Failed starting file transfer!\n");
    }

    // Shutdown client connection and cleanup
    KineticClient_DestroyConnection(&session);
    KineticClient_Shutdown(client);
}


static int put_chunk_of_file(FileTransferProgress* transfer)
{
    AsyncWriteClosureData* closureData = calloc(1, sizeof(AsyncWriteClosureData));
    transfer->opsInProgress++;
    closureData->currentTransfer = transfer;

    size_t bytesRead = fread(closureData->value, 1, sizeof(closureData->value), transfer->fp);
    bool eofReached = feof(transfer->fp) != 0;
    if (bytesRead > 0) {
        transfer->currentChunk++;
        closureData->entry = (KineticEntry){
            .key = ByteBuffer_CreateAndAppend(closureData->key, sizeof(closureData->key),
                &transfer->keyPrefix, sizeof(transfer->keyPrefix)),
            .tag = ByteBuffer_CreateAndAppendFormattedCString(closureData->tag, sizeof(closureData->tag),
                "some_value_tag..._%04d", transfer->currentChunk),
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = ByteBuffer_Create(closureData->value, sizeof(closureData->value), (size_t)bytesRead),
            .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
        };

        // Make sure last write has FLUSH enabled to ensure all data is written
        if (eofReached) {
            closureData->entry.synchronization = KINETIC_SYNCHRONIZATION_FLUSH;
        }

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
        else {
            transfer->bytesWritten += bytesRead;
        }
    }
    else if (bytesRead == 0) {
        if (eofReached) {
            transfer->opsInProgress--;
        }
        else {
            transfer->opsInProgress--;
            fprintf(stderr, "Failed reading data from file! error: %s\n", strerror(errno));
        }
        free(closureData);
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

FileTransferProgress * start_file_transfer(KineticSession * const session,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks)
{
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed opening data file '%s'! error: %s\n", filename, strerror(errno));
        return NULL;
    }

    FileTransferProgress * transferState = malloc(sizeof(FileTransferProgress));
    *transferState = (FileTransferProgress) {
        .session = session,
        .maxOverlappedChunks = maxOverlappedChunks,
        .keyPrefix = keyPrefix,
        .fp = fp,
    };

    pthread_mutex_init(&transferState->transferMutex, NULL);
    pthread_mutex_init(&transferState->completeMutex, NULL);
    pthread_cond_init(&transferState->completeCond, NULL);
    gettimeofday(&transferState->startTime, NULL);
        
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
    
    fclose(transfer->fp);
    transfer->fp = NULL;
    struct timeval stopTime;
    gettimeofday(&stopTime, NULL);

    pthread_mutex_destroy(&transfer->completeMutex);
    pthread_cond_destroy(&transfer->completeCond);

    int64_t elapsed_us = ((stopTime.tv_sec - transfer->startTime.tv_sec) * 1000000)
        + (stopTime.tv_usec - transfer->startTime.tv_usec);
    float elapsed_ms = elapsed_us / 1000.0f;
    float bandwidth = (transfer->bytesWritten * 1000.0f) / (elapsed_ms * 1024 * 1024);
    fflush(stdout);
    printf("\n"
        "Write/Put Performance:\n"
        "----------------------------------------\n"
        "wrote:      %.1f kB\n"
        "duration:   %.3f seconds\n"
        "throughput: %.2f MB/sec\n\n",
        transfer->bytesWritten / 1024.0f,
        elapsed_ms / 1000.0f,
        bandwidth);

    KineticStatus status = transfer->status;
    free(transfer);

    return status;
}
