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

static void do_put_and_getprevious_getnext(KineticSession *session) {
    for (int i = 0; i < 3; i++) {
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

    printf("\n\n\n");

    for (int i = 1; i < 3; i++) {
        KineticStatus status = KINETIC_STATUS_INVALID;
        static const ssize_t sz = 100;
        char key_buf[sz];
        char tag_buf[sz];
        char value_buf[sz];
        ByteBuffer keyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key%d", i);
        ByteBuffer tagBuffer = ByteBuffer_CreateAndAppendFormattedCString(tag_buf, sz, "tag%d", i);
        ByteBuffer valueBuffer = ByteBuffer_Create(value_buf, sz, 0);

        KineticEntry entry = {
            .key = keyBuffer,
            .tag = tagBuffer,
            .value = valueBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
        };

        if (i > 0) {
            status = KineticClient_GetPrevious(session, &entry, NULL);
            printf("GetPrevious status: %s\n", Kinetic_GetStatusDescription(status));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("Previous key before 'key%d': '%s', value '%s'\n",
                    i, key_buf, value_buf);
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        KineticStatus status = KINETIC_STATUS_INVALID;
        static const ssize_t sz = 100;
        char key_buf[sz];
        char tag_buf[sz];
        char value_buf[sz];
        ByteBuffer keyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key%d", i);
        ByteBuffer tagBuffer = ByteBuffer_CreateAndAppendFormattedCString(tag_buf, sz, "tag%d", i);
        ByteBuffer valueBuffer = ByteBuffer_Create(value_buf, sz, 0);

        KineticEntry entry = {
            .key = keyBuffer,
            .tag = tagBuffer,
            .value = valueBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
        };

        if (i < 2) {
            status = KineticClient_GetNext(session, &entry, NULL);
            printf("GetNext status: %s\n", Kinetic_GetStatusDescription(status));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("Next key after 'key%d': '%s', value '%s'\n",
                    i, key_buf, value_buf);
            }
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

    do_put_and_getprevious_getnext(session);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
    return 0;
}
