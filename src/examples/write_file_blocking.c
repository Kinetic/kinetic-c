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

typedef struct {
    char ip[16];
    KineticSession* session;
    char keyPrefix[KINETIC_DEFAULT_KEY_LEN];
    uint8_t key[KINETIC_DEFAULT_KEY_LEN];
    uint8_t version[KINETIC_DEFAULT_KEY_LEN];
    uint8_t tag[KINETIC_DEFAULT_KEY_LEN];
    uint8_t value[KINETIC_OBJ_SIZE];
    KineticEntry entry;
    ByteBuffer data;
    KineticStatus status;
} write_args;

void store_data(write_args* args)
{
    KineticEntry* entry = &(args->entry);
    int32_t objIndex = 0;

    while (ByteBuffer_BytesRemaining(args->data) > 0) {

        // Configure entry meta-data
        ByteBuffer_Reset(&entry->key);
        ByteBuffer_AppendCString(&entry->key, args->keyPrefix);
        char keySuffix[8];
        snprintf(keySuffix, sizeof(keySuffix), "%02d", objIndex);
        ByteBuffer_AppendCString(&entry->key, keySuffix);

        // Prepare entry with the next object to store
        ByteBuffer_Reset(&entry->value);
        ByteBuffer_AppendArray(
            &entry->value,
            ByteBuffer_Consume(
                &args->data,
                MIN(ByteBuffer_BytesRemaining(args->data), KINETIC_OBJ_SIZE))
        );

        // Store the object
        KineticStatus status = KineticClient_Put(args->session, entry, NULL);
        if (status != KINETIC_STATUS_SUCCESS) {
            fprintf(stderr, "Kinetic PUT of object %d to host %s failed w/ status: %s\n",
                objIndex, args->ip, Kinetic_GetStatusDescription(status));
            exit(-1);
        }

        objIndex++;
    }

    printf("File stored on Kinetic Device across %d entries\n", objIndex);
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

    // Read in file contents to store
    const char* dataFile = "test/support/data/test.data";
    struct stat st;
    stat(dataFile, &st);
    char* buf = malloc(st.st_size);
    int fd = open(dataFile, O_RDONLY);
    long dataLen = read(fd, buf, st.st_size);
    close(fd);
    if (dataLen <= 0) {
        fprintf(stderr, "Failed reading data file to store: %s\n", dataFile);
        exit(-1);
    }


    write_args* writeArgs = calloc(1, sizeof(write_args));
    writeArgs->session = session;

    // Create a ByteBuffer for consuming chunks of data out of for overlapped PUTs
    writeArgs->data = ByteBuffer_Create(buf, dataLen, 0);

    // Configure common meta-data for the entries
    struct timeval now;
    gettimeofday(&now, NULL);
    snprintf(writeArgs->keyPrefix, sizeof(writeArgs->keyPrefix), "%010ld_", now.tv_sec);
    ByteBuffer verBuf = ByteBuffer_Create(writeArgs->version, sizeof(writeArgs->version), 0);
    ByteBuffer_AppendCString(&verBuf, "v1.0");
    ByteBuffer tagBuf = ByteBuffer_Create(writeArgs->tag, sizeof(writeArgs->tag), 0);
    ByteBuffer_AppendCString(&tagBuf, "some_value_tag...");
    writeArgs->entry = (KineticEntry) {
        .key = ByteBuffer_Create(writeArgs->key, sizeof(writeArgs->key), 0),
        // .newVersion = verBuf,
        .tag = tagBuf,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ByteBuffer_Create(writeArgs->value, sizeof(writeArgs->value), 0),
        .synchronization = KINETIC_SYNCHRONIZATION_WRITEBACK,
    };
    strcpy(writeArgs->ip, sessionConfig.host);

    // Store the data
    printf("\nWriting data file to the Kinetic device...\n");
    store_data(writeArgs);

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(writeArgs->session);
    KineticClient_Shutdown(client);
    free(writeArgs);
    free(buf);

    return 0;
}
