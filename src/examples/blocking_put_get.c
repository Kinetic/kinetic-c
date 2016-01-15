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
#include <ctype.h>

#include <openssl/sha.h>

static void do_put_and_get(KineticSession *session) {
    const char key[] = "key";
    ByteBuffer put_key_buf = ByteBuffer_MallocAndAppend(key, strlen(key));

    const uint8_t value[] = "value\x01\x02\x03\x04";
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

    /* Put "key" => "value\x01\x02\x03\x04".
     * This will block, because the callback field (arg 3) is NULL. */
    KineticStatus status = KineticClient_Put(session, &put_entry, NULL);
    printf("Put status: %s\n", Kinetic_GetStatusDescription(status));

    /* Allocate a tag large enough for the SHA1. */
    ByteBuffer get_tag_buf = ByteBuffer_Malloc(put_tag_buf.bytesUsed);
    /* Allocate a buffer large enough for the value. */
    ByteBuffer get_value_buf = ByteBuffer_Malloc(put_value_buf.bytesUsed);

    KineticEntry get_entry = {
        .key = put_key_buf,
        .value = get_value_buf,
        .tag = get_tag_buf,
        .algorithm = KINETIC_ALGORITHM_SHA1,
    };

    status = KineticClient_Get(session, &get_entry, NULL);
    printf("Get status: %s\n", Kinetic_GetStatusDescription(status));
    printf("Get value: %zd bytes\n    ", get_entry.value.bytesUsed);
    for (size_t i = 0; i < get_entry.value.bytesUsed; i++) {
        printf("%02x ", get_entry.value.array.data[i]);
    }
    printf("    ");
    for (size_t i = 0; i < get_entry.value.bytesUsed; i++) {
        char c = get_entry.value.array.data[i];
        printf("%c", isprint(c) ? c : '.');
    }
    printf("\n");

    /* Cleanup */
    ByteBuffer_Free(put_key_buf);
    ByteBuffer_Free(put_value_buf);
    ByteBuffer_Free(put_tag_buf);
    ByteBuffer_Free(get_entry.value);
    ByteBuffer_Free(get_tag_buf);
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

    do_put_and_get(session);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
    return 0;
}
