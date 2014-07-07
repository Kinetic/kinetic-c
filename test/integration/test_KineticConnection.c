#include "KineticConnection.h"
#include "unity.h"
#include "mock_KineticNetwork.h"

#include <netinet/in.h>
#include <ifaddrs.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticConnection_Create_should_create_a_new_connection_object(void)
{
    KineticConnection connection = KineticConnection_Create();
    TEST_ASSERT(sizeof(connection) > 0);
    TEST_ASSERT_FALSE(connection.Connected);
}

void test_KineticConnection_Connect_should_connect_to_supplied_IP_address(void)
{
    struct sockaddr destIface;
    struct sockaddr destNetMask;
    struct sockaddr destAddr;
    struct ifaddrs dest = {
        NULL,
        "My Connection",
        0x12345678,
        &destIface,
        &destNetMask,
        &destAddr,
        NULL
    };

    KineticConnection connection = KineticConnection_Create();

    KineticNetwork_GetDestinationIP_ExpectAndReturn(dest);

    TEST_ASSERT_EQUAL(KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS, KineticConnection_Connect(&connection));
    TEST_ASSERT_TRUE(connection.Connected);
    TEST_ASSERT_EQUAL_STRING("My Connection", connection.Name);
    TEST_ASSERT_EQUAL_MEMORY(&destIface, &connection.Addr, sizeof(destIface));
    TEST_ASSERT_EQUAL_MEMORY(&destNetMask, &connection.NetMask, sizeof(destNetMask));
    TEST_ASSERT_EQUAL_MEMORY(&destAddr, &connection.DestAddr, sizeof(destAddr));
}
