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
#include <openssl/sha.h>
#include <pthread.h>

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} PutStatus;

static void put_finished(KineticCompletionData* kinetic_data, void* clientData);

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
        .host = "127.0.0.1",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticStatus connect_status = KineticClient_CreateSession(&config, client, &session);
    if (connect_status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(connect_status));
        return 1;
    }

    // Create structure to populate with PUT status in callback
    //   a semaphore is used to notify the main thread that it's
    //   safe to proceed.
    PutStatus put_status = {
        .sem = KineticSemaphore_Create(),
        .status = KINETIC_STATUS_INVALID,
    };

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

    // Because I'm passing a pointer to this entry to KineticClient_Put(), this entry must not
    //   go out of scope until the PUT completes
    KineticEntry entry = {
        .key = key,
        .tag = tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(
        session,
        &entry,
        &(KineticCompletionClosure) {
            .callback = put_finished,
            .clientData = &put_status,
        }
    );

    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        return 1;
    }

    // Wait for PUT to finish
    KineticSemaphore_WaitForSignalAndDestroy(put_status.sem);

    if (put_status.status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(put_status.status));
        return 1;
    }
    printf("PUT completed successfully!\n");

    // Free malloc'd buffers
    ByteBuffer_Free(value);
    ByteBuffer_Free(key);
    ByteBuffer_Free(tag);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);

    return 0;
}

static void put_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    PutStatus * put_status = clientData;

    // Save PUT result status
    put_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(put_status->sem);
}
