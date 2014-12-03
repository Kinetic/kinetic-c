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
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>

#define REPORT_ERRNO(en, msg) if(en != 0){errno = en; perror(msg);}

typedef struct {
    size_t opsInProgress;
    int fd;
    KineticStatus status;
    KineticSessionHandle sessionHandle;
} FileTransferProgress;

typedef struct {
    KineticEntry entry;
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    FileTransferProgress* currentTransfer;
} AsyncWriteClosureData;

static FileTransferProgress * start_file_transfer(KineticSessionHandle handle,
    char const * const filename, uint64_t keyPrefix);
static KineticStatus wait_for_transfer_complete(FileTransferProgress* const transfer);

static void put_complete(KineticCompletionData* kinetic_data, void* client_data);


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    const KineticSession sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .no_threads = true,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticClient_Init("stdout", 0);

    // Establish connection
    KineticSessionHandle sessionHandle;
    KineticStatus status = KineticClient_Connect(&sessionConfig, &sessionHandle);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        return -1;
    }

    // Create a unique/common key prefix
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t prefix = (uint64_t)now.tv_sec << 8;

    // Kick off the write/PUT operations and wait for completion
    const char* dataFile = "test/support/data/test.data";
    FileTransferProgress* transfer = start_file_transfer(sessionHandle, dataFile, prefix);
    if (transfer != NULL) {
        printf("Waiting for transfer to complete...\n");
        status = wait_for_transfer_complete(transfer);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Transfer failed w/status: %s\n", Kinetic_GetStatusDescription(status));
            return -2;
        }
        printf("Transfer completed successfully!\n");
    }

    // Shutdown client connection and cleanup
    KineticClient_Disconnect(&sessionHandle);
    KineticClient_Shutdown();

    return 0;
}

static void put_complete(KineticCompletionData* kinetic_data, void* clientData)
{
    AsyncWriteClosureData* closureData = clientData;
    FileTransferProgress* currentTransfer = closureData->currentTransfer;
    free(closureData);
    currentTransfer->opsInProgress--;

    // make sure the first error (if there is one) is the one we keep
    if (currentTransfer->status == KINETIC_STATUS_SUCCESS) {
        currentTransfer->status = kinetic_data->status;
    }

    if (kinetic_data->status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed writing chunk! PUT response reported status: %s\n",
            Kinetic_GetStatusDescription(kinetic_data->status));
    }
}

static FileTransferProgress * start_file_transfer(KineticSessionHandle handle,
    char const * const filename, uint64_t keyPrefix)
{
    FileTransferProgress * transferState = calloc(1, sizeof(FileTransferProgress));
    int file = open(filename, O_RDONLY);
    *transferState = (FileTransferProgress) {
        .opsInProgress = 0,
        .status = KINETIC_STATUS_SUCCESS,
        .sessionHandle = handle,
        .fd = file,
    };

    if (file < 0) {
        printf("Unable to open %s\n", filename);
        return NULL;
    }

    struct stat inputfile_stat;
    fstat(file, &inputfile_stat);
    char* inputfile_data = (char*)mmap(0, inputfile_stat.st_size, PROT_READ, MAP_SHARED, file, 0);
   
    for (off_t i = 0; i < inputfile_stat.st_size; i += 1024*1024) {
       int value_size = 1024*1024;
       if (i + value_size > inputfile_stat.st_size) {
           value_size = inputfile_stat.st_size - i + 1;
       }
       AsyncWriteClosureData* closureData = calloc(1, sizeof(AsyncWriteClosureData));
       int32_t currentChunk = (i >> 20);
       uint64_t key = keyPrefix + currentChunk;

       closureData->entry = (KineticEntry){
           .key = ByteBuffer_CreateAndAppend(closureData->key, sizeof(closureData->key),
               &key, sizeof(key)),
           .tag = ByteBuffer_CreateAndAppendFormattedCString(closureData->tag, sizeof(closureData->tag),
               "some_value_tag..._%04d", currentChunk),
           .algorithm = KINETIC_ALGORITHM_SHA1,
           .value = ByteBuffer_Create(&inputfile_data[i], value_size, value_size),
           .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
       };
       closureData->currentTransfer = transferState;
       transferState->opsInProgress++;
       KineticStatus status = KineticClient_Put(handle,
           &closureData->entry,
           &(KineticCompletionClosure) {
               .callback = put_complete,
               .clientData = closureData,
           });
       if (status != KINETIC_STATUS_SUCCESS) {
           transferState->opsInProgress--;
           free(closureData);
           fprintf(stderr, "Failed writing chunk! PUT request reported status: %s\n",
               Kinetic_GetStatusDescription(status));
       }

    }
    return transferState;
}

static KineticStatus wait_for_transfer_complete(FileTransferProgress* const transfer)
{
    while (transfer->opsInProgress > 0) {
        if (KineticClient_AsyncRun(&transfer->sessionHandle) != KINETIC_STATUS_SUCCESS) {
            break;
        }
    }

    KineticStatus status = transfer->status;

    close(transfer->fd);

    free(transfer);

    return status;
}
