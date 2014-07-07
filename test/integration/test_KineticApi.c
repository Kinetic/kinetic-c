#include "KineticApi.h"
#include "unity.h"
#include "kinetic.h"
#include "mock_KineticConnection.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticApi_Connect_should_create_a_connection(void)
{
    KineticConnection expected, actual;
    expected.Connected = true;

    KineticConnection_Create_ExpectAndReturn(expected);

    actual = KineticApi_Connect();

    TEST_ASSERT(sizeof(actual) > 0);
    TEST_ASSERT_EQUAL_MEMORY(&expected, &actual, sizeof(expected));
}

void test_KineticApi_SendNoop_should_send_NOOP_command(void)
{
    KineticConnection connection;
    KineticProto_Status_StatusCode status;

    KineticConnection_Create_ExpectAndReturn(connection);

    connection = KineticApi_Connect();
    status = KineticApi_SendNoop(&connection);

    TEST_IGNORE_MESSAGE("Finish me!");

    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, status);
}
