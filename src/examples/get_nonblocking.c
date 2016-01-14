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
#include "kinetic_semaphore.h"
#include <stdlib.h>
#include <sys/param.h>
#include <openssl/sha.h>
#include <pthread.h>

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} GetStatus;

static void get_finished(KineticCompletionData* kinetic_data, void* clientData);

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
        exit(1);
    }

    // some dummy data to PUT
    uint8_t value_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
    ByteBuffer value = ByteBuffer_MallocAndAppend(value_data, sizeof(value_data));

    // a dummy key
    uint8_t key_data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    ByteBuffer key = ByteBuffer_MallocAndAppend(key_data, sizeof(key_data));

    // Populate tag with SHA1
    ByteBuffer tag = ByteBuffer_Malloc(20);
    uint8_t sha1[20];
    SHA1(value.array.data, value.bytesUsed, &sha1[0]);
    ByteBuffer_Append(&tag, sha1, sizeof(sha1));

    KineticEntry entry = {
        .key = key,
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    // Do a blocking put to make sure there is something there to read back
    KineticStatus put_status = KineticClient_Put(session, &entry, NULL);

    if (put_status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Put failed w/status: %s\n", Kinetic_GetStatusDescription(put_status));
        return 1;
    }

    // Create structure to populate with GET status in callback
    //   a semaphore is used to notify the main thread that it's
    //   safe to proceed.
    GetStatus get_status = {
        .sem = KineticSemaphore_Create(),
        .status = KINETIC_STATUS_INVALID,
    };

    ByteBuffer getTag = ByteBuffer_Malloc(tag.bytesUsed);
    ByteBuffer getValue = ByteBuffer_Malloc(value.bytesUsed);

    // Because I'm passing a pointer to this entry to KineticClient_Put(), this entry must not
    //   go out of scope until the GET completes
    KineticEntry get_entry = {
        .key = key,
        .tag = getTag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = getValue,
        .force = true,
    };

    status = KineticClient_Get(
        session,
        &get_entry,
        &(KineticCompletionClosure) {
            .callback = get_finished,
            .clientData = &get_status,
        }
    );
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Get failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        return 1;
    }

    // Wait for GET to finish
    KineticSemaphore_WaitForSignalAndDestroy(get_status.sem);

    if (get_status.status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(get_status.status));
        return 1;
    }

    if ((value.bytesUsed == getValue.bytesUsed) &&
        (memcmp(value.array.data, getValue.array.data, getValue.bytesUsed) != 0)) {
        fprintf(stderr, "GET completed but returned unexpected value");
        return 1;
    }
    printf("GET completed successfully!\n");

    // Free malloc'd buffers
    ByteBuffer_Free(value);
    ByteBuffer_Free(key);
    ByteBuffer_Free(tag);

    ByteBuffer_Free(getValue);
    ByteBuffer_Free(getTag);


    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);

    return 0;
}

static void get_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    GetStatus * get_status = clientData;

    // Save GET result status
    get_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(get_status->sem);
}
