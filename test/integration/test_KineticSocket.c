#include "KineticSocket.h"
#include "KineticLogger.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
    KineticLogger_Init(KINETIC_LOG_FILE);
}

void tearDown(void)
{
}

void test_KineticSocket_KINETIC_PORT_should_be_8213(void)
{
    TEST_ASSERT_EQUAL(8213, KINETIC_PORT);
}

void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    int fd = KineticSocket_Connect("localhost", KINETIC_PORT, true);

    TEST_ASSERT_MESSAGE(fd >= 0, "File descriptor invalid");

    KineticSocket_Close(fd);
}
