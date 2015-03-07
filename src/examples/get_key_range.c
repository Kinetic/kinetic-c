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
#include "byte_array.h"
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

static bool create_entries(KineticSession * const session, const int count);

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

    // Create some entries so that we can query the keys
    printf("Storing some entries on the device...\n");
    const size_t numKeys = 3;
    if (!create_entries(session, numKeys)) {
        return 2;
    }

    // Query a range of keys
    const size_t keyLen = 64;
    uint8_t startKeyData[keyLen], endKeyData[keyLen];
    KineticKeyRange range = {
        .startKey = ByteBuffer_CreateAndAppendCString(startKeyData, sizeof(startKeyData), "key_prefix_00"),
        .endKey = ByteBuffer_CreateAndAppendCString(endKeyData, sizeof(endKeyData), "key_prefix_01"),
        .startKeyInclusive = true,
        .endKeyInclusive = true,
        .maxReturned = 3,
    };
    uint8_t keysData[numKeys][keyLen];
    ByteBuffer keyBuff[] = {
        ByteBuffer_Create(&keysData[0], keyLen, 0),
        ByteBuffer_Create(&keysData[1], keyLen, 0),
        ByteBuffer_Create(&keysData[2], keyLen, 0),
    };
    ByteBufferArray keys = {.buffers = &keyBuff[0], .count = numKeys};

    status = KineticClient_GetKeyRange(session, &range, &keys, NULL);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "FAILURE: Failed retrieving key range from device!\n");
        return 3;
    }
    
    if (keys.used != 2) {
        fprintf(stderr, "FAILURE: Unexpected number of keys in returned range!\n");
        return 4;
    };
    if (keyBuff[0].bytesUsed != strlen("key_prefix_00")) {
        fprintf(stderr, "FAILURE: Key 0 length check failed!\n");
        return 4;
    }
    if (keyBuff[1].bytesUsed != strlen("key_prefix_01")) {
        fprintf(stderr, "FAILURE: Key 1 length check failed!\n");
        return 4;
    }
    if (keyBuff[2].bytesUsed != 0) {
        fprintf(stderr, "FAILURE: Key 2 was not empty as expected!\n");
        return 4;
    }

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
    printf("Key range retrieved successfully!\n");

    return 0;
}

static bool create_entries(KineticSession * const session, const int count)
{
    static const ssize_t sz = 20;
    char key_buf[sz];
    char value_buf[sz];

    for (int i = 0; i < count; i++) {

        ByteBuffer KeyBuffer = ByteBuffer_CreateAndAppendFormattedCString(key_buf, sz, "key_prefix_%02d", i);
        ByteBuffer ValueBuffer = ByteBuffer_CreateAndAppendFormattedCString(value_buf, sz, "val_%02d", i);

        KineticEntry entry = {
            .key = KeyBuffer,
            .value = ValueBuffer,
            .algorithm = KINETIC_ALGORITHM_SHA1,
            .force = true,
        };

        KineticStatus status = KineticClient_Put(session, &entry, NULL);
        if (KINETIC_STATUS_SUCCESS != status) { return false; }
    }

    return true;
}
