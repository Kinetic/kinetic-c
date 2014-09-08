#ifndef _SYSTEM_TEST_FIXTURE
#define _SYSTEM_TEST_FIXTURE

#include "kinetic_types.h"

typedef struct _SystemTestInstance
{
    bool testIgnored;
    KineticOperation operation;
} SystemTestInstance;

typedef struct _SystemTestFixture
{
    char host[HOST_NAME_MAX];
    int port;
    int64_t clusterVersion;
    int64_t identity;
    bool nonBlocking;
    int64_t expectedSequence;
    ByteArray hmacKey;
    bool connected;
    KineticConnection connection;
    KineticPDU request;
    KineticPDU response;
    SystemTestInstance instance;
} SystemTestFixture;

void SystemTestSetup(SystemTestFixture* fixture);
void SystemTestTearDown(SystemTestFixture* fixture);
void SystemTestSuiteTearDown(SystemTestFixture* fixture);

#define SYSTEM_TEST_SUITE_TEARDOWN(_fixture) \
void test_Suite_TearDown(void) \
{ \
    /*TEST_ASSERT_NOT_NULL_MESSAGE((_fixture), "System test fixture passed to 'SYSTEM_TEST_SUITE_TEARDOWN' is NULL!");*/ \
    if ((_fixture)->connected) \
        KineticClient_Disconnect(&(_fixture)->connection); \
    (_fixture)->connected = false; \
    (_fixture)->instance.testIgnored = true; \
}

#endif // _SYSTEM_TEST_FIXTURE
