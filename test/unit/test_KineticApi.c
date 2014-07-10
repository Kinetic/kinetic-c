#include "KineticApi.h"
#include "unity.h"
#include "kinetic.h"
#include "mock_KineticLogger.h"
#include "mock_KineticConnection.h"

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
    KineticProto_Status_StatusCode status;

    KineticConnection_Create_ExpectAndReturn(connection);
    KineticConnection_Connect_ExpectAndReturn(&connection, "salgood.com", 88, false, true);

    connection = KineticApi_Connect("salgood.com", 88, false);
    status = KineticApi_SendNoop(&connection);

    TEST_IGNORE_MESSAGE("Finish me!");

    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
}
