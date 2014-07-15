#include "KineticApi.h"
#include "unity.h"
#include "unity_helper.h"
#include <stdio.h>
#include <protobuf-c/protobuf-c.h>
#include "KineticProto.h"
#include "mock_KineticMessage.h"
#include "mock_KineticLogger.h"
#include "mock_KineticConnection.h"
#include "mock_KineticExchange.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticApi_Init_should_initialize_the_logger(void)
{
    KineticLogger_Init_Expect("some/file.log");

    KineticApi_Init("some/file.log");
}

void test_KineticApi_Connect_should_create_a_connection(void)
{
    KineticConnection dummy;
    KineticConnection result;

    dummy.Connected = false; // Ensure gets set appropriately by internal connect call

    KineticConnection_Create_ExpectAndReturn(dummy);
    KineticConnection_Connect_ExpectAndReturn(&dummy, "somehost.com", 321, true, true);

    result = KineticApi_Connect("somehost.com", 321, true);

    TEST_ASSERT_TRUE(result.Connected);
}

void test_KineticApi_Connect_should_log_a_failed_connection(void)
{
    KineticConnection dummy;
    KineticConnection result;

    // Ensure appropriately updated per internal connect call result
    dummy.Connected = true;
    dummy.FileDescriptor = 333;

    KineticConnection_Create_ExpectAndReturn(dummy);
    KineticConnection_Connect_ExpectAndReturn(&dummy, "somehost.com", 123, true, false);
    KineticLogger_Log_Expect("Failed creating connection to somehost.com:123");

    result = KineticApi_Connect("somehost.com", 123, true);

    TEST_ASSERT_FALSE(result.Connected);
    TEST_ASSERT_EQUAL(-1, result.FileDescriptor);
}

void test_KineticApi_SendNoop_should_send_NOOP_command(void)
{
    KineticConnection connection;
    KineticExchange exchange;
    KineticMessage request, response;
    KineticProto_Status_StatusCode status;
    int64_t identity = 1234;
    int64_t connectionID = 5678;
    request.exchange = &exchange;

    KineticConnection_Create_ExpectAndReturn(connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "salgood.com", 88, false, true);

    connection = KineticApi_Connect("salgood.com", 88, false);

    KineticExchange_Init_Expect(&exchange, identity, connectionID, &connection);
    KineticMessage_Init_Expect(&request, &exchange);
    KineticMessage_BuildNoop_Expect(&request);
    KineticConnection_SendMessage_ExpectAndReturn(&connection, &request, true);
    KineticConnection_ReceiveMessage_ExpectAndReturn(&connection, &response, true);

    status = KineticApi_SendNoop(&connection, &request, &response);

    TEST_IGNORE_MESSAGE("Finish me!!!");

    TEST_ASSERT_EQUAL_KINETIC_STATUS(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&exchange, response.exchange);
}
