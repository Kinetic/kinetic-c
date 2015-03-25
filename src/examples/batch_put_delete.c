/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include <stdlib.h>
#include <openssl/sha.h>

static void put_delete_finished(KineticCompletionData* kinetic_data, void* client_data)
{
	assert(kinetic_data->status == KINETIC_STATUS_SUCCESS);
	assert(client_data == NULL);
}

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
        exit(1);
    }

    //prepare data for foo entry
    uint8_t foo_value_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
    ByteBuffer foo_value = ByteBuffer_MallocAndAppend(foo_value_data, sizeof(foo_value_data));
    uint8_t foo_key_data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    ByteBuffer foo_key = ByteBuffer_MallocAndAppend(foo_key_data, sizeof(foo_key_data));
    ByteBuffer foo_tag = ByteBuffer_Malloc(20);
    uint8_t foo_sha1[20];
    SHA1(foo_value.array.data, foo_value.bytesUsed, &foo_sha1[0]);
    ByteBuffer_Append(&foo_tag, foo_sha1, sizeof(foo_sha1));

    //prapare data for bar entry
    uint8_t bar_value_data[] = { 0x01, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
    ByteBuffer bar_value = ByteBuffer_MallocAndAppend(bar_value_data, sizeof(bar_value_data));
    uint8_t bar_key_data[] = {0x01, 0x00, 0x02, 0x03, 0x04};
    ByteBuffer bar_key = ByteBuffer_MallocAndAppend(bar_key_data, sizeof(bar_key_data));
    ByteBuffer bar_tag = ByteBuffer_Malloc(20);
    uint8_t bar_sha1[20];
    SHA1(bar_value.array.data, bar_value.bytesUsed, &bar_sha1[0]);
    ByteBuffer_Append(&bar_tag, bar_sha1, sizeof(bar_sha1));

    KineticEntry put_foo_entry = {
        .key = foo_key,
        .tag = foo_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = foo_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Put(session,&put_foo_entry,NULL);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        exit(1);
    }

    // Delete foo entry and put bar entry in a batch
    KineticBatch_Operation * batchOp = KineticClient_InitBatchOperation(session);
    KineticEntry delete_foo_entry = {
    	.key = foo_key,
    };

    status = KineticClient_BatchStart(batchOp);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Start batch failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        exit(1);
    }

    status = KineticClient_BatchDelete(batchOp, &delete_foo_entry, &(KineticCompletionClosure) {
        .callback = put_delete_finished,
        .clientData = NULL,
    });
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Batch delete failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        exit(1);
    };

    KineticEntry put_bar_entry = {
        .key = bar_key,
        .tag = bar_tag,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = bar_value,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_BatchPut(batchOp, &put_bar_entry, &(KineticCompletionClosure) {
        .callback = put_delete_finished,
        .clientData = NULL,
    });
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Batch put failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        exit(1);
    };

    status = KineticClient_BatchEnd(batchOp);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "End batch failed w/status: %s\n", Kinetic_GetStatusDescription(status));
        exit(1);
    };

    // Free buffers
    ByteBuffer_Free(foo_value);
    ByteBuffer_Free(foo_key);
    ByteBuffer_Free(foo_tag);
    ByteBuffer_Free(bar_value);
    ByteBuffer_Free(bar_key);
    ByteBuffer_Free(bar_tag);

    // Free batch operation
    free(batchOp);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);

    return 0;
}
