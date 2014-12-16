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
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_device_info.h"
#include "kinetic_serial_allocator.h"
#include "kinetic_proto.h"
#include "kinetic_allocator.h"
#include "kinetic_message.h"
#include "kinetic_pdu.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_connection.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

static SystemTestFixture Fixture;

static ByteBuffer Key1Buffer;
static ByteBuffer Value1Buffer;
static ByteBuffer Key2Buffer;
static ByteBuffer Value2Buffer;
static ByteBuffer TagBuffer;
static ByteBuffer VersionBuffer;

static uint8_t dummyTagData[] = {0x00};

static uint8_t key1Data[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
static uint8_t value1Data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
static uint8_t key2Data[] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};
static uint8_t value2Data[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
static uint8_t versionData[] = {0x00, 0x00, 0x00, 0x01};

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
static ByteBuffer ReadVersionBuffer;

static uint8_t readValueData[1024];
static uint8_t readTagData[1024];
static uint8_t readVersionData[1024];

void setUp(void)
{ LOG_LOCATION;
    SystemTestSetup(&Fixture);




    Key1Buffer    = ByteBuffer_Create(key1Data, sizeof(key1Data), sizeof(key1Data));
    Value1Buffer  = ByteBuffer_Create(value1Data, sizeof(value1Data), sizeof(value1Data));
    Key2Buffer    = ByteBuffer_Create(key2Data, sizeof(key2Data), sizeof(key2Data));
    Value2Buffer  = ByteBuffer_Create(value2Data, sizeof(value2Data), sizeof(value2Data));
    TagBuffer     = ByteBuffer_Create(dummyTagData, sizeof(dummyTagData), sizeof(dummyTagData));
    VersionBuffer = ByteBuffer_Create(versionData, sizeof(versionData), sizeof(versionData));

    ReadValueBuffer   = ByteBuffer_Create(readValueData, sizeof(readValueData), 0);
    ReadTagBuffer     = ByteBuffer_Create(readTagData, sizeof(readTagData), 0);
    ReadVersionBuffer = ByteBuffer_Create(readVersionData, sizeof(readVersionData), 0);


    Key3Buffer = ByteBuffer_Create(key3Data, sizeof(key3Data), sizeof(key3Data));
    Key4Buffer = ByteBuffer_Create(key4Data, sizeof(key4Data), sizeof(key4Data));

    Key5Buffer = ByteBuffer_Create(key5Data, sizeof(key5Data), sizeof(key5Data));
    Key6Buffer = ByteBuffer_Create(key6Data, sizeof(key6Data), sizeof(key6Data));

    // Setup to write some test data
    KineticEntry putEntry1 = {
        .key = Key1Buffer,
        .tag = TagBuffer,
        .newVersion = VersionBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = Value1Buffer,
        .force = true,
    };

    KineticStatus status = KineticClient_Put(&Fixture.session, &putEntry1, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    KineticEntry putEntry2 = {
        .key = Key2Buffer,
        .tag = TagBuffer,
        .newVersion = VersionBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = Value2Buffer,
        .force = true,
    };

    status = KineticClient_Put(&Fixture.session, &putEntry2, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    Fixture.expectedSequence++;
    sleep(1);
}

void tearDown(void)
{ LOG_LOCATION;
    SystemTestTearDown(&Fixture);
}

// These tests are disabled until we have a way of running them on travis CI
//  Currently we can only run these by starting a simulator on two different computers
#if 0
void test_P2P_should_copy_keys_from_one_device_to_another(void)
{ LOG_LOCATION;


    ByteBuffer_Reset(&ReadValueBuffer);
    ByteBuffer_Reset(&ReadTagBuffer);
    ByteBuffer_Reset(&ReadVersionBuffer);

    KineticEntry getEntry1 = {
        .key = Key1Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Get(&Fixture.session, &getEntry1, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry1.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key1Buffer, getEntry1.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry1.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1.value);


    KineticEntry getEntry2 = {
        .key = Key2Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(&Fixture.session, &getEntry2, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry2.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry2.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key2Buffer, getEntry2.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry2.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2.value);




    ByteArray hmacArray = ByteArray_CreateWithCString("asdfasdf");
    
    KineticSessionConfig config = {
        .host = SYSTEM_TEST_HOST2,
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity =  1,
        .hmacKey = hmacArray,
    };

    KineticSession session;
    status = KineticClient_CreateConnection(&config, &session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Erase the drive
    status = KineticClient_InstantSecureErase(session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);








    KineticP2P_OperationData ops[] ={
        {
            .key = Key1Buffer,
            .version = VersionBuffer,
            .newKey = Key3Buffer,
        },
        {
            .key = Key2Buffer,
            .newKey = Key4Buffer,
        }
    };

    KineticP2P_Operation p2pOp = {
        .peer.hostname = SYSTEM_TEST_HOST2,
        .peer.port = KINETIC_PORT,
        .peer.tls = false,
        .numOperations = NUM_ELEMENTS(ops),
        .operations = ops
    };

    status = KineticClient_P2POperation(&Fixture.session, &p2pOp, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[1].resultStatus);








    ByteBuffer_Reset(&ReadValueBuffer);
    ByteBuffer_Reset(&ReadTagBuffer);
    ByteBuffer_Reset(&ReadVersionBuffer);

    KineticEntry getEntry1asdf = {
        .key = Key3Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(session, &getEntry1asdf, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry1asdf.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1asdf.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key3Buffer, getEntry1asdf.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1asdf.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry1asdf.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1asdf.value);


    KineticEntry getEntry2asdf = {
        .key = Key4Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(session, &getEntry2asdf, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry2asdf.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry2asdf.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key4Buffer, getEntry2asdf.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2asdf.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry2asdf.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2asdf.value);




    status = KineticClient_Disconnect(&session);
    TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when disconnecting client!");
}



void test_P2P_should_support_nesting_of_p2p_operations(void)
{ LOG_LOCATION;
    ByteBuffer_Reset(&ReadValueBuffer);
    ByteBuffer_Reset(&ReadTagBuffer);
    ByteBuffer_Reset(&ReadVersionBuffer);

    KineticEntry getEntry1 = {
        .key = Key1Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    KineticStatus status = KineticClient_Get(&Fixture.session, &getEntry1, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry1.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key1Buffer, getEntry1.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry1.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1.value);


    KineticEntry getEntry2 = {
        .key = Key2Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(&Fixture.session, &getEntry2, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry2.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry2.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key2Buffer, getEntry2.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry2.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2.value);




    ByteArray hmacArray = ByteArray_CreateWithCString("asdfasdf");
    
    KineticSession session = {
        .config =  {
            .host = SYSTEM_TEST_HOST2,
            .port = KINETIC_PORT,
            .clusterVersion = 0,
            .identity =  1,
            .hmacKey = hmacArray,
        },
    };
    status = KineticClient_CreateConnection(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    // Erase the drive
    status = KineticClient_InstantSecureErase(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);



    KineticP2P_OperationData ops_copy_back1[] = {
        {
            .key = Key1Buffer,
            .newKey = Key5Buffer,
        }
    };

    KineticP2P_OperationData ops_copy_back2[] = {
        {
            .key = Key2Buffer,
            .newKey = Key6Buffer,
        }
    };

    KineticP2P_Operation p2pOp_copy_back1 = {
        .peer.hostname = "asdfasdgwergewf",
        .peer.port = KINETIC_PORT,
        .peer.tls = false,
        .numOperations = NUM_ELEMENTS(ops_copy_back1),
        .operations = ops_copy_back1
    };

    KineticP2P_Operation p2pOp_copy_back2 = {
        .peer.hostname = "asdfasdggssdagsadgf",
        .peer.port = KINETIC_PORT,
        .peer.tls = false,
        .numOperations = NUM_ELEMENTS(ops_copy_back2),
        .operations = ops_copy_back2
    };

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
        .peer.hostname = SYSTEM_TEST_HOST2,
        .peer.port = KINETIC_PORT,
        .peer.tls = false,
        .numOperations = NUM_ELEMENTS(ops_copy_there),
        .operations = ops_copy_there
    };

    

    status = KineticClient_P2POperation(&Fixture.session, &p2pOp, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[1].resultStatus);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[0].chainedOperation->operations[0].resultStatus);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, p2pOp.operations[1].chainedOperation->operations[0].resultStatus);









    ByteBuffer_Reset(&ReadValueBuffer);
    ByteBuffer_Reset(&ReadTagBuffer);
    ByteBuffer_Reset(&ReadVersionBuffer);

    KineticEntry getEntry1asdf = {
        .key = Key1Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(&Fixture.session, &getEntry1asdf, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry1asdf.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry1asdf.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key3Buffer, getEntry1asdf.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry1asdf.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry1asdf.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value1Buffer, getEntry1asdf.value);


    KineticEntry getEntry2asdf = {
        .key = Key2Buffer,
        .dbVersion = ReadVersionBuffer,
        .tag = ReadTagBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ReadValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(&Fixture.session, &getEntry2asdf, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_ByteBuffer(VersionBuffer, getEntry2asdf.dbVersion);
    TEST_ASSERT_ByteBuffer_NULL(getEntry2asdf.newVersion);
    TEST_ASSERT_EQUAL_ByteBuffer(Key4Buffer, getEntry2asdf.key);
    TEST_ASSERT_EQUAL_ByteBuffer(TagBuffer, getEntry2asdf.tag);
    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry2asdf.algorithm);
    TEST_ASSERT_EQUAL_ByteBuffer(Value2Buffer, getEntry2asdf.value);




    status = KineticClient_DestroyConnection(&session);
    TEST_ASSERT_EQUAL_MESSAGE(KINETIC_STATUS_SUCCESS, status, "Error when disconnecting client!");
}
#endif

void test_P2P_should_fail_with_a_buffer_overrun_error_if_to_many_operations_specified(void)
{ LOG_LOCATION;

    size_t to_many_operations = 100000;
    KineticP2P_OperationData * ops = calloc(to_many_operations, sizeof(KineticP2P_OperationData));
    for (size_t i = 0; i < to_many_operations; i++)
    {
        ops[i] = (KineticP2P_OperationData){
            .key = Key1Buffer,
            .newKey = Key3Buffer,
        };
    }

    KineticP2P_Operation p2pOp = {
        .peer.hostname = SYSTEM_TEST_HOST,
        .peer.port = KINETIC_PORT,
        .peer.tls = false,
        .numOperations = to_many_operations,
        .operations = ops
    };

    KineticStatus status = KineticClient_P2POperation(&Fixture.session, &p2pOp, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_BUFFER_OVERRUN, status);

    free(ops);
}


void test_P2P_should_fail_with_a_operation_invalid_if_to_many_chained_p2p_operations(void)
{ LOG_LOCATION;

    size_t to_many_operations = 1001;
    KineticP2P_OperationData * ops = calloc(to_many_operations, sizeof(KineticP2P_OperationData));
    KineticP2P_Operation * chained_ops = calloc(to_many_operations, sizeof(KineticP2P_Operation));

    for (size_t i = 0; i < to_many_operations; i++)
    {
        ops[i] = (KineticP2P_OperationData){
            .key = Key1Buffer,
            .newKey = Key3Buffer,
            .chainedOperation = (i == (to_many_operations - 1)) ? NULL : &chained_ops[i + 1],
        };
        chained_ops[i] = (KineticP2P_Operation){
            .peer.hostname = SYSTEM_TEST_HOST,
            .peer.port = KINETIC_PORT,
            .peer.tls = false,
            .numOperations = 1,
            .operations = &ops[i],
        };
    }

    KineticStatus status = KineticClient_P2POperation(&Fixture.session, &chained_ops[0], NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_OPERATION_INVALID, status);

    free(ops);
    free(chained_ops);
}


/*******************************************************************************
* ENSURE THIS IS AFTER ALL TESTS IN THE TEST SUITE
*******************************************************************************/
SYSTEM_TEST_SUITE_TEARDOWN(&Fixture)
