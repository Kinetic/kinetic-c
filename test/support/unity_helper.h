#include "unity.h"

/** Custom Unity assertion which validates that a  */
#define TEST_ASSERT_EQUAL_KINETIC_STATUS(expected, actual) \
if (expected != actual) { \
    char err[128]; \
    sprintf(err, "Expected Kinetic status code of %s(%d), Was %s(%d)", \
        protobuf_c_enum_descriptor_get_value( \
            &KineticProto_status_status_code_descriptor, expected)->name, \
        expected, \
        protobuf_c_enum_descriptor_get_value( \
            &KineticProto_status_status_code_descriptor, actual)->name, \
        actual); \
    TEST_FAIL_MESSAGE(err); \
}

/** Custom Unity assertion which validates the expected int64_t value is
    packed properly into a buffer in Network Byte-Order (big endian) */
#define TEST_ASSERT_EQUAL_NBO_INT64(expected, buf) { \
    int i; int64_t val = 0; char err[64];\
    for(i = 0; i < sizeof(int64_t); i++) {val <<= 8; val += buf[i]; } \
    sprintf(err, "@ index %d", i); \
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, val, err); \
}
