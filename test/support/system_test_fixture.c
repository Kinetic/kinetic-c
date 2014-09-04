#include "system_test_fixture.h"
#include "unity_helper.h"
#include "kinetic_client.h"
#include "kinetic_logger.h"

uint8_t hmacKeyBuffer[KINETIC_MAX_KEY_LEN];
uint8_t data[PDU_VALUE_MAX_LEN];

void SystemTestSetup(SystemTestFixture* fixture)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");
    fixture->instance.testIgnored = false;

    if (!fixture->connected)
    {
        KineticClient_Init(NULL);

        KINETIC_CONNECTION_INIT(&fixture->connection,
            fixture->identity, fixture->hmacKey);

        bool success = KineticClient_Connect(
            &fixture->connection,
            fixture->host, fixture->port, fixture->nonBlocking,
            fixture->clusterVersion, fixture->identity, fixture->hmacKey);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT(fixture->connection.socketDescriptor >= 0);
        fixture->expectedSequence = 0;
        fixture->connected = true;
    }

    fixture->instance.operation =
        KineticClient_CreateOperation(&fixture->connection,
        &fixture->request, &fixture->response);

    KINETIC_PDU_INIT_WITH_MESSAGE(&fixture->request, &fixture->connection);
    KINETIC_PDU_INIT(&fixture->response, &fixture->connection);

    TEST_ASSERT_EQUAL_MESSAGE(
        fixture->expectedSequence,
        fixture->connection.sequence,
        "Failed validating starting sequence count for the"
        " operation w/session!");
}

void SystemTestTearDown(SystemTestFixture* fixture)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "System test fixture is NULL!");

    if (!fixture->instance.testIgnored)
    {
        fixture->expectedSequence++;
        TEST_ASSERT_EQUAL_MESSAGE(
            fixture->expectedSequence,
            fixture->connection.sequence,
            "Sequence should post-increment for every operation on the session!");
    }
}
