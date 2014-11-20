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
#include <sys/file.h>
#include <errno.h>

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
    KineticSessionHandle sessionHandle;
} FileTransferProgress;

typedef struct {
    KineticEntry entry;
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t value[KINETIC_OBJ_SIZE];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    FileTransferProgress* currentTransfer;
} AsyncWriteClosureData;

FileTransferProgress * start_file_transfer(KineticSessionHandle handle,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks);
KineticStatus wait_for_put_finish(FileTransferProgress* const transfer);

static int put_chunk_of_file(FileTransferProgress* transfer);
static void put_chunk_of_file_finished(KineticCompletionData* kinetic_data, void* client_data);


void test_kinetic_client_should_store_a_binary_object_split_across_entries_via_ovelapped_asynchronous_IO_operations(void)
{
    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    const KineticSession sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticClient_Init("stdout", 0);

    // Establish connection
    KineticSessionHandle sessionHandle;
    KineticStatus status = KineticClient_Connect(&sessionConfig, &sessionHandle);
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
    FileTransferProgress* transfer = start_file_transfer(sessionHandle, dataFile, prefix, 4);
    printf("Waiting for transfer to complete...\n");
    status = wait_for_put_finish(transfer);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Transfer failed w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }
    printf("Transfer completed successfully!\n");

    // Shutdown client connection and cleanup
    KineticClient_Disconnect(&sessionHandle);
    KineticClient_Shutdown();
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
        KineticStatus status = KineticClient_Put(transfer->sessionHandle,
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

FileTransferProgress * start_file_transfer(KineticSessionHandle handle,
    char const * const filename, uint64_t keyPrefix, uint32_t maxOverlappedChunks)
{
    FileTransferProgress * transferState = malloc(sizeof(FileTransferProgress));
    *transferState = (FileTransferProgress) {
        .sessionHandle = handle,
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
