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
#include <ctype.h>

#include <openssl/sha.h>

static void do_put_and_getkeyrange(KineticSession *session) {
    for (int i = 0; i < 5; i++) {
        char key[] = "keyX";
        key[3] = '0' + i;
        ByteBuffer put_key_buf = ByteBuffer_MallocAndAppend(key, strlen(key));
        
        uint8_t value[] = "valueX";
        value[5] = '0' + i;
        ByteBuffer put_value_buf = ByteBuffer_MallocAndAppend(value, sizeof(value));
        
        /* Populate tag with SHA1 of value */
        ByteBuffer put_tag_buf = ByteBuffer_Malloc(20);
        uint8_t sha1[20];
        SHA1(put_value_buf.array.data, put_value_buf.bytesUsed, &sha1[0]);
        ByteBuffer_Append(&put_tag_buf, sha1, sizeof(sha1));
        
        KineticEntry put_entry = {
            .key = put_key_buf,
            .value = put_value_buf,
            .tag = put_tag_buf,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            /* Set sync to WRITETHROUGH, which will wait to complete
             * until the drive has persistend the write. (WRITEBACK
             * returns as soon as the drive has buffered the write.) */
            .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };

        /* Put "keyX" => "valueX", where 'X' is 0..4.
         * This will block, because the callback field (arg 3) is NULL. */
        KineticStatus status = KineticClient_Put(session, &put_entry, NULL);
        printf("Put status: %s\n", Kinetic_GetStatusDescription(status));

        ByteBuffer_Free(put_key_buf);
        ByteBuffer_Free(put_value_buf);
        ByteBuffer_Free(put_tag_buf);
    }

    const size_t max_key_count = 5;
    const size_t max_key_length = 64;
    uint8_t first_key[max_key_length];
    uint8_t last_key[max_key_length];

    KineticKeyRange range = {
        .startKey = ByteBuffer_CreateAndAppendCString(first_key, sizeof(first_key), "key"),
        .endKey = ByteBuffer_CreateAndAppendCString(last_key, sizeof(last_key), "key\xFF"),
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = max_key_count,
    };
    
    uint8_t key_mem[max_key_count][max_key_length];
    memset(key_mem, 0, sizeof(key_mem));

    ByteBuffer key_buffers[max_key_count];
    for (size_t i = 0; i < max_key_count; i++) {
        key_buffers[i] = ByteBuffer_Create(&key_buffers[i], max_key_length, 0);
    }
    ByteBufferArray keys = {
        .buffers = key_buffers,
        .count = max_key_count,
    };

    /* Request the key range as specified in &range, populating the keys in &keys. */
    KineticStatus status = KineticClient_GetKeyRange(session, &range, &keys, NULL);
    printf("GetKeyRange status: %s\n", Kinetic_GetStatusDescription(status));
    
    if (status == KINETIC_STATUS_SUCCESS) {
        for (size_t i = 0; i < max_key_count; i++) {
            printf("%zd: %s\n", i, key_buffers[i].array.data);
        }
    }

    /* No cleanup necessary */
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
        exit(1);
    }

    do_put_and_getkeyrange(session);
    
    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
    return 0;
}
