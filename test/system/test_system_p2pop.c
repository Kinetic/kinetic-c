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
#include "kinetic_admin_client.h"
#include <stdlib.h>

static ByteBuffer Key1Buffer;
static ByteBuffer Value1Buffer;
static ByteBuffer Key2Buffer;
static ByteBuffer Value2Buffer;
static ByteBuffer TagBuffer;

static uint8_t dummyTagData[] = {0x00};

static uint8_t key1Data[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
static uint8_t value1Data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
static uint8_t key2Data[] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};
static uint8_t value2Data[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static ByteBuffer Key3Buffer;
static ByteBuffer Key4Buffer;

static ByteBuffer Key5Buffer;
static ByteBuffer Key6Buffer;

static uint8_t key3Data[] = {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};
static uint8_t key4Data[] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};

static uint8_t key5Data[] = {0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05};
static uint8_t key6Data[] = {0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06};

static ByteBuffer ReadValueBuffer;
static ByteBuffer ReadTagBuffer;

static uint8_t readValueData[1024];
static uint8_t readTagData[1024];

static KineticClient * client;
static KineticSession * session;
static KineticSession * adminSession;
static KineticSession * peerSession;
static KineticSession * peerAdminSession;

static const char HmacKeyString[] = "asdfasdf";

void setUp(void)
{
    KineticClientConfig clientConfig = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    client = KineticClient_Init(&clientConfig);

    // Create sessions with primary device
    KineticSessionConfig sessionConfig = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(sessionConfig.host, GetSystemTestHost1(), sizeof(sessionConfig.host)-1);
    sessionConfig.port = GetSystemTestPort1();
    KineticStatus status = KineticClient_CreateSession(&sessionConfig, client, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    KineticSessionConfig adminSessionConfig = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
        .useSsl = true,
    };
    strncpy(adminSessionConfig.host, GetSystemTestHost1(), sizeof(adminSessionConfig.host)-1);
    adminSessionConfig.port = GetSystemTestTlsPort1();
    status = KineticAdminClient_CreateSession(&adminSessionConfig, client, &adminSession);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Create sessions with peer device
    KineticSessionConfig peerConfig = {
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(peerConfig.host, GetSystemTestHost2(), sizeof(peerConfig.host)-1);
    peerConfig.port = GetSystemTestPort2();
    status = KineticClient_CreateSession(&peerConfig, client, &peerSession);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    KineticSessionConfig peerAdminConfig = {
        .clusterVersion = 0,
        .useSsl = true,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    strncpy(peerAdminConfig.host, GetSystemTestHost2(), sizeof(peerAdminConfig.host)-1);
    peerAdminConfig.port = GetSystemTestTlsPort2();
    status = KineticAdminClient_CreateSession(&peerAdminConfig, client, &peerAdminSession);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Create some test entries to have content to copy
    TagBuffer     = ByteBuffer_Create(dummyTagData, sizeof(dummyTagData), sizeof(dummyTagData));
    Key1Buffer    = ByteBuffer_Create(key1Data, sizeof(key1Data), sizeof(key1Data));
    Value1Buffer  = ByteBuffer_Create(value1Data, sizeof(value1Data), sizeof(value1Data));
    KineticEntry putEntry1 = {
        .key = Key1Buffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = Value1Buffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Put(session, &putEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    Key2Buffer    = ByteBuffer_Create(key2Data, sizeof(key2Data), sizeof(key2Data));
    Value2Buffer  = ByteBuffer_Create(value2Data, sizeof(value2Data), sizeof(value2Data));
    KineticEntry putEntry2 = {
        .key = Key2Buffer,
        .tag = TagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = Value2Buffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    status = KineticClient_Put(session, &putEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    ReadValueBuffer = ByteBuffer_Create(readValueData, sizeof(readValueData), 0);
    ReadTagBuffer   = ByteBuffer_Create(readTagData, sizeof(readTagData), 0);
    Key3Buffer = ByteBuffer_Create(key3Data, sizeof(key3Data), sizeof(key3Data));
    Key4Buffer = ByteBuffer_Create(key4Data, sizeof(key4Data), sizeof(key4Data));
    Key5Buffer = ByteBuffer_Create(key5Data, sizeof(key5Data), sizeof(key5Data));
    Key6Buffer = ByteBuffer_Create(key6Data, sizeof(key6Data), sizeof(key6Data));
}

void tearDown(void)
{
    KineticStatus status = KineticClient_DestroySession(session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    status = KineticClient_DestroySession(adminSession);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
    status = KineticClient_DestroySession(peerSession);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);
    status = KineticClient_DestroySession(peerAdminSession);
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, status);

    KineticClient_Shutdown(client);
}

void test_P2P_should_copy_keys_from_one_device_to_another(void)
{
    KineticEntry getEntry1 = {
        .key = Key1Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    KineticStatus status = KineticClient_Get(session, &getEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(Key1Buffer, getEntry1.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry1.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1.value);

    KineticEntry getEntry2 = {
        .key = Key2Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(session, &getEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(Key2Buffer, getEntry2.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry2.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2.value);

    ByteArray oldPin = ByteArray_CreateWithCString("");
    ByteArray newPin = ByteArray_CreateWithCString("1234");

    // Erase the peer drive we are copying to
    if (!SystemTestIsUnderSimulator()) {
        status = KineticAdminClient_SetErasePin(peerAdminSession, oldPin, newPin);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    status = KineticAdminClient_SecureErase(peerAdminSession, newPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Execute a P2P operation to copy the entries
    KineticP2P_OperationData ops[] = {
        {.key = Key1Buffer, .newKey = Key3Buffer},
        {.key = Key2Buffer, .newKey = Key4Buffer}
    };
    KineticP2P_Operation p2pOp = {
        .peer = {.tls = false},
        .numOperations = NUM_ELEMENTS(ops),
        .operations = ops
    };
    p2pOp.peer.hostname = (char*)GetSystemTestHost2();
    p2pOp.peer.port = GetSystemTestPort2();
    status = KineticClient_P2POperation(session, &p2pOp, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[1].resultStatus);

    // Read the copies to validate their existence
    KineticEntry getEntry1Copy = {
        .key = Key3Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(peerSession, &getEntry1Copy, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1Copy.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key3Buffer, getEntry1Copy.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1Copy.tag);
    KineticEntry getEntry2Copy = {
        .key = Key4Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(peerSession, &getEntry2Copy, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(Key4Buffer, getEntry2Copy.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2Copy.tag);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2Copy.value);
}

// This test is disabled at the moment because the simulator does not currently support nested operations.
//   It works on real hardware
void disabled_test_P2P_should_support_nesting_of_p2p_operations(void)
{
    KineticEntry getEntry1 = {
        .key = Key1Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    KineticStatus status = KineticClient_Get(session, &getEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    KineticEntry getEntry2 = {
        .key = Key2Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(session, &getEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    ByteArray oldPin = ByteArray_CreateWithCString("");
    ByteArray newPin = ByteArray_CreateWithCString("1234");

    // Erase the peer drive we are copying to
    if (!SystemTestIsUnderSimulator()) {
        status = KineticAdminClient_SetErasePin(peerAdminSession, oldPin, newPin);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
    status = KineticAdminClient_SecureErase(peerAdminSession, newPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Copy the entries back to primary device
    KineticP2P_OperationData ops_copy_back1[] = {
        {.key = Key3Buffer, .newKey = Key5Buffer}
    };
    KineticP2P_Operation p2pOp_copy_back1 = {
        .peer = {.tls = false},
        .numOperations = NUM_ELEMENTS(ops_copy_back1),
        .operations = ops_copy_back1
    };
    p2pOp_copy_back1.peer.hostname = (char*)GetSystemTestHost1();
    p2pOp_copy_back1.peer.port = GetSystemTestPort1();

    KineticP2P_OperationData ops_copy_back2[] = {
        {.key = Key4Buffer, .newKey = Key6Buffer}
    };
    KineticP2P_Operation p2pOp_copy_back2 = {
        .peer = {.tls = false},
        .numOperations = NUM_ELEMENTS(ops_copy_back2),
        .operations = ops_copy_back2
    };
    p2pOp_copy_back2.peer.hostname = (char*)GetSystemTestHost1();
    p2pOp_copy_back2.peer.port = GetSystemTestPort1();

    KineticP2P_OperationData ops_copy_there[] = {
        {
            .key = Key1Buffer,
            .newKey = Key3Buffer,
            .chainedOperation = &p2pOp_copy_back1,
        },
        {
            .key = Key2Buffer,
            .newKey = Key4Buffer,
            .chainedOperation = &p2pOp_copy_back2,
        }
    };

    KineticP2P_Operation p2pOp = {
        .peer = {.tls = false},
        .numOperations = NUM_ELEMENTS(ops_copy_there),
        .operations = ops_copy_there
    };
    p2pOp.peer.hostname = (char*)GetSystemTestHost2();
    p2pOp.peer.port = GetSystemTestPort2();
    status = KineticClient_P2POperation(session, &p2pOp, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
        status);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
        p2pOp.operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
        p2pOp.operations[1].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
        p2pOp.operations[0].chainedOperation->operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS,
        p2pOp.operations[1].chainedOperation->operations[0].resultStatus);

    // Read the copied entries from primary device
    KineticEntry getEntry1Copy = {
        .key = Key5Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(session, &getEntry1Copy, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1Copy.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key5Buffer, getEntry1Copy.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1Copy.tag);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1Copy.value);
    KineticEntry getEntry2Copy = {
        .key = Key6Buffer,
        .tag = ReadTagBuffer,
        .value = ReadValueBuffer,
    };
    status = KineticClient_Get(session, &getEntry2Copy, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_ByteBuffer_NULL(getEntry2Copy.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key6Buffer, getEntry2Copy.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2Copy.tag);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2Copy.value);
}

void test_P2P_should_fail_with_a_buffer_overrun_error_if_too_many_operations_specified(void)
{
    size_t too_many_operations = KINETIC_P2P_OPERATION_LIMIT;
    KineticP2P_OperationData * ops = calloc(too_many_operations, sizeof(KineticP2P_OperationData));
    for (size_t i = 0; i < too_many_operations; i++) {
        ops[i] = (KineticP2P_OperationData) {
            .key = Key1Buffer,
            .newKey = Key3Buffer,
        };
    }

    KineticP2P_Operation p2pOp = {
        .peer = {.tls = false},
        .numOperations = too_many_operations,
        .operations = ops
    };
    p2pOp.peer.hostname = (char*)GetSystemTestHost1();
    p2pOp.peer.port = GetSystemTestPort1();

    KineticStatus status = KineticClient_P2POperation(session, &p2pOp, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);

    free(ops);
}

void test_P2P_should_fail_with_a_operation_invalid_if_too_many_chained_p2p_operations(void)
{
    size_t too_many_operations = KINETIC_P2P_OPERATION_LIMIT + 1;
    KineticP2P_OperationData * ops = calloc(too_many_operations, sizeof(KineticP2P_OperationData));
    KineticP2P_Operation * chained_ops = calloc(too_many_operations, sizeof(KineticP2P_Operation));

    for (size_t i = 0; i < too_many_operations; i++) {
        ops[i] = (KineticP2P_OperationData){
            .key = Key1Buffer,
            .newKey = Key3Buffer,
            .chainedOperation = (i == (too_many_operations - 1)) ? NULL : &chained_ops[i + 1],
        };
        chained_ops[i] = (KineticP2P_Operation){
            .peer = {.tls = false},
            .numOperations = 1,
            .operations = &ops[i],
        };
        chained_ops[i].peer.hostname = (char*)GetSystemTestHost1();
        chained_ops[i].peer.port = GetSystemTestPort1();
    }

    KineticStatus status = KineticClient_P2POperation(session, &chained_ops[0], NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_INVALID, status);

    free(ops);
    free(chained_ops);
}

