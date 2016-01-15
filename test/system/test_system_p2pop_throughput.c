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

#include "system_test_fixture.h"
#include "kinetic_client.h"
#include "kinetic_semaphore.h"
#include <stdlib.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>

void run_p2p_throughput_test(KineticClient * client, size_t num_ops, size_t value_size)
{
    printf("\n"
        "========================================\n"
        "P2P Throughput Test\n"
        "========================================\n"
        "Entry Size: %zu bytes\n"
        "Count:      %zu entries\n",
        value_size, num_ops);

    ByteBuffer test_data = ByteBuffer_Malloc(value_size);
    ByteBuffer_AppendDummyData(&test_data, test_data.array.len);

    // Create session with primary device
    const char HmacKeyString[] = "asdfasdf";
    KineticSession* session1;
    KineticSessionConfig config1 = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(config1.host, GetSystemTestHost1(), sizeof(config1.host)-1);
    config1.port = GetSystemTestPort1();
    KineticStatus status = KineticClient_CreateSession(&config1, client, &session1);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

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
            TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
        }
    }

    // Configure session for peer device
    KineticSessionConfig config2 = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(config2.host, GetSystemTestHost2(), sizeof(config2.host)-1);
    config2.port = GetSystemTestPort2();

    // P2P copy to another drive
    printf("Starting P2P copy of %ld entries from %s:%d, to %s:%d\n",
        (long)num_ops, config1.host, (int)config1.port, config2.host, (int)config2.port);
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
            .peer = {.tls = false},
            .numOperations = num_ops,
            .operations = ops
        };
        p2pOp.peer.hostname = (char*)GetSystemTestHost2();
        p2pOp.peer.port = GetSystemTestPort2();

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

    // Establish connection to peer device
    printf("GET'ing copied keys from %s:%d to verify copy was successful\n", config2.host, (int)config2.port);
    KineticSession* session2;
    status = KineticClient_CreateSession(&config2, client, &session2);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // GET values to verify that the copy succeeded
    {
        ByteBuffer test_get_datas[num_ops];
        for (size_t i = 0; i < num_ops; i++) {
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
            TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
            TEST_ASSERT_EQUAL_ByteBuffer(test_data, entries[get].value);

            entries[get] = (KineticEntry) {
                .key = key,
                .tag = tag,
                .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
                .force = true,
            };

            // Delete keys to clean up after ourselves
            status = KineticClient_Delete(session2, &entries[get], NULL);
            TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
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
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&config);
    run_p2p_throughput_test(client, 50, KINETIC_OBJ_SIZE);
    KineticClient_Shutdown(client);
}
