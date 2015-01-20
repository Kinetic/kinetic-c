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
#include "kinetic_semaphore.h"
#include <stdlib.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
// Smaller number of large payloads
#define NUM_PUTS (500)
#define PAYLOAD_SIZE (KINETIC_OBJ_SIZE)

// Large number of very small payloads
//#define NUM_PUTS (10000)
//#define PAYLOAD_SIZE (20)

typedef struct {
    KineticSemaphore * sem;
    KineticStatus status;
} PutStatus;

static void put_finished(KineticCompletionData* kinetic_data, void* clientData);

void test_kinetic_client_should_store_a_binary_object_split_across_entries_via_ovelapped_asynchronous_IO_operations(void)
{
    ByteBuffer test_data = ByteBuffer_Malloc(PAYLOAD_SIZE);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    // Establish session with device
    KineticClient * client = KineticClient_Init("stdout", 0);
    const char HmacKeyString[] = "asdfasdf";
    KineticSessionConfig config = {
        .host = SYSTEM_TEST_HOST,
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticSession* session;
    KineticStatus status = KineticClient_CreateSession(&config, client, &session);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }

    uint8_t tag_data[] = {0x00, 0x01, 0x02, 0x03};
    ByteBuffer tag = ByteBuffer_Create(tag_data, sizeof(tag_data), sizeof(tag_data));

    PutStatus put_statuses[NUM_PUTS];
    for (size_t i = 0; i < NUM_PUTS; i++) {
        put_statuses[i] = (PutStatus){
            .sem = KineticSemaphore_Create(),
            .status = KINETIC_STATUS_INVALID,
        };
    };

    uint32_t keys[NUM_PUTS];
    KineticEntry entries[NUM_PUTS];

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    for (uint32_t put = 0; put < NUM_PUTS; put++) {
        keys[put] = put;
        ByteBuffer key = ByteBuffer_Create(&keys[put], sizeof(keys[put]), sizeof(keys[put]));

        KineticSynchronization sync = (put == NUM_PUTS - 1)
            ? KINETIC_SYNCHRONIZATION_FLUSH
            : KINETIC_SYNCHRONIZATION_WRITEBACK;

        entries[put] = (KineticEntry) {
            .key = key,
            .tag = tag,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .value = test_data,
            .synchronization = sync,
        };

        KineticStatus status = KineticClient_Put(session,
            &entries[put],
            &(KineticCompletionClosure) {
                .callback = put_finished,
                .clientData = &put_statuses[put],
            }
        );

        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
            TEST_FAIL();
        }
    }

    printf("Waiting for put finish\n");

    for (size_t i = 0; i < NUM_PUTS; i++)
    {
        KineticSemaphore_WaitForSignalAndDestroy(put_statuses[i].sem);
        if (put_statuses[i].status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(put_statuses[i].status));
            TEST_FAIL();
        }
    }

    struct timeval stop_time;
    gettimeofday(&stop_time, NULL);

    size_t bytes_written = NUM_PUTS * test_data.array.len;
    int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
        + (stop_time.tv_usec - start_time.tv_usec);
    float elapsed_ms = elapsed_us / 1000.0f;
    float bandwidth = (bytes_written * 1000.0f) / (elapsed_ms * 1024 * 1024);
    fflush(stdout);
    printf("\n"
        "Write/Put Performance:\n"
        "----------------------------------------\n"
        "wrote:      %.1f kB\n"
        "duration:   %.3f seconds\n"
        "throughput: %.2f MB/sec\n\n",
        bytes_written / 1024.0f,
        elapsed_ms / 1000.0f,
        bandwidth);
    
    printf("Transfer completed successfully!\n");

    ByteBuffer_Free(test_data);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
}

static void put_finished(KineticCompletionData* kinetic_data, void* clientData)
{
    PutStatus * put_status = clientData;
    // Save PUT result status
    put_status->status = kinetic_data->status;
    // Signal that we're done
    KineticSemaphore_Signal(put_status->sem);
}
