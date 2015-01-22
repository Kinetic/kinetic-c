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

#define KINETIC_TEST_PORT1            (8123)
#define KINETIC_TEST_PORT2            (8124)

#define KINETIC_TEST_HOST1  "localhost"
#define KINETIC_TEST_HOST2  "localhost"

// #define KINETIC_TEST_PORT1            (8123)
// #define KINETIC_TEST_PORT2            (8123)

// #define KINETIC_TEST_HOST1  "10.138.123.78"
// #define KINETIC_TEST_HOST2  "10.138.123.128"


void run_p2p_throughput_test(KineticClient * client, size_t num_ops, size_t value_size)
{
    printf("\n"
        "========================================\n"
        "P2P Throughput Test\n"
        "========================================\n"
        "Entry Size: %zu bytes\n"
        "Count:      %zu entries\n",
        value_size, num_ops );

    ByteBuffer test_data = ByteBuffer_Malloc(value_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    // Initialize kinetic-c and configure sessions
    const char HmacKeyString[] = "asdfasdf";
    KineticSession* session1;
    KineticSessionConfig config1 = {
        .host = KINETIC_TEST_HOST1,
        .port = KINETIC_TEST_PORT1,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    // Establish connection
    KineticStatus status = KineticClient_CreateSession(&config1, client, &session1);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }

    uint8_t tag_data[] = {0x00, 0x01, 0x02, 0x03};
    ByteBuffer tag = ByteBuffer_Create(tag_data, sizeof(tag_data), sizeof(tag_data));

    uint64_t keys[num_ops];
    KineticEntry entries[num_ops];

    for (uint32_t put = 0; put < num_ops; put++) {
        keys[put] = put;
    }

    printf("PUT'ing %ld, %ld byte entries to %s:%d\n",
        (long) num_ops, (long)value_size, config1.host, (int)config1.port);
    // Put some keys to copy
    {
        for (uint32_t put = 0; put < num_ops; put++) {
            ByteBuffer key = ByteBuffer_Create(&keys[put], sizeof(keys[put]), sizeof(keys[put]));

            entries[put] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .algorithm = KINETIC_ALGORITHM_SHA1,
                .value = test_data,
                .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
            };

            KineticStatus status = KineticClient_Put(session1, &entries[put], NULL);

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "PUT failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }
    }


    KineticSession* session2;
    KineticSessionConfig config2 = {
        .host = KINETIC_TEST_HOST2,
        .port = KINETIC_TEST_PORT2,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };

    printf("Starting P2P copy of %ld entries from %s:%d, to %s:%d\n",
        (long)num_ops, config1.host, (int)config1.port, config2.host, (int)config2.port);
    // P2P copy to another drive
    {
        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        KineticP2P_OperationData ops[num_ops];
        for (uint32_t i = 0; i < num_ops; i++) {
            ByteBuffer key = ByteBuffer_Create(&keys[i], sizeof(keys[i]), sizeof(keys[i]));
            ops[i] = (KineticP2P_OperationData) {
                .key = key,
                .newKey = key,
            };
        }

        KineticP2P_Operation p2pOp = {
            .peer = {
                .hostname = KINETIC_TEST_HOST2,
                .port = KINETIC_TEST_PORT2,
                .tls = false,
            },
            .numOperations = num_ops,
            .operations = ops
        };

        status = KineticClient_P2POperation(session1, &p2pOp, NULL);

        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

        struct timeval stop_time;
        gettimeofday(&stop_time, NULL);

        size_t bytes_copied = num_ops * test_data.array.len;


        int64_t elapsed_us = ((stop_time.tv_sec - start_time.tv_sec) * 1000000)
            + (stop_time.tv_usec - start_time.tv_usec);
        float elapsed_ms = elapsed_us / 1000.0f;
        float bandwidth = (bytes_copied * 1000.0f) / (elapsed_ms * 1024 * 1024);
        fflush(stdout);
        printf("\n"
            "P2P Performance:\n"
            "----------------------------------------\n"
            "bytes copied:      %.1f kB\n"
            "duration:   %.3f seconds\n"
            "throughput: %.2f MB/sec\n\n",
            bytes_copied / 1024.0f,
            elapsed_ms / 1000.0f,
            bandwidth);
    }

    printf("GET'ing copied keys from %s:%d to verify copy was successful\n", config2.host, (int)config2.port);
    // Establish connection
    status = KineticClient_CreateSession(&config2, client, &session2);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        TEST_FAIL();
    }

    // GET values to verify that the copy succeeded
    {
        ByteBuffer test_get_datas[num_ops];
        for (size_t i = 0; i < num_ops; i++)
        {
            test_get_datas[i] = ByteBuffer_Malloc(value_size);
        }

        for (uint32_t get = 0; get < num_ops; get++) {
            ByteBuffer key = ByteBuffer_Create(&keys[get], sizeof(keys[get]), sizeof(keys[get]));

            entries[get] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .value = test_get_datas[get],
            };

            status = KineticClient_Get(session2, &entries[get], NULL);
            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "GET failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
            TEST_ASSERT_EQUAL_ByteBuffer(test_data, entries[get].value);

            entries[get] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
                .force = true,
            };

            // Delete keys to clean up after ourselves
            status = KineticClient_Delete(session2, &entries[get], NULL);

            if (status != KINETIC_STATUS_SUCCESS) {
                fprintf(stderr, "DELETE failed w/status: %s\n", Kinetic_GetStatusDescription(status));
                TEST_FAIL();
            }
        }

        for (size_t i = 0; i < num_ops; i++) {
            ByteBuffer_Free(test_get_datas[i]);
        }
    }

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session1);
    KineticClient_DestroySession(session2);
    ByteBuffer_Free(test_data);
}

void test_p2p_throughput(void)
{
    KineticClientConfig config = {
        .logFile = "stdout",
        .logLevel = 0,
    };
    KineticClient * client = KineticClient_Init(&config);
    run_p2p_throughput_test(client, 50, KINETIC_OBJ_SIZE);
    KineticClient_Shutdown(client);   
}
