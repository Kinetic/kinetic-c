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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_auth.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

KineticPDU PDU;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    memset(&PDU, 0, sizeof(PDU));
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAuth_EnsurePinSupplied_should_return_SUCCESS_if_pin_supplied(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .pin = ByteArray_Create(&session.config.pinData[0], sizeof(session.config.pinData)),
            .port = 1234
        }
    };
    strcpy((char*)session.config.pinData, "192736aHUx@*G!Q");

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, KineticAuth_EnsurePinSupplied(&session));
}

void test_KineticAuth_EnsurePinSupplied_should_return_PIN_REQUIRED_if_pin_not_supplied(void)
{
    KineticSession session = {.config = (KineticSessionConfig) {.port = 1234}};

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_PIN_REQUIRED, KineticAuth_EnsurePinSupplied(&session));
}



void test_KineticAuth_EnsureSslEnabled_should_return_SUCCESS_if_pin_supplied(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = true,
            .port = 1234
        }
    };
    strcpy((char*)session.config.pinData, "192736aHUx@*G!Q");

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, KineticAuth_EnsureSslEnabled(&session));
}

void test_KineticAuth_EnsureSslEnabled_should_return_SSL_REQUIRED_if_pin_not_supplied(void)
{
    KineticSession session = {.config = (KineticSessionConfig) {.port = 1234}};

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, KineticAuth_EnsureSslEnabled(&session));
}



void test_KineticAuth_Populate_should_return_SSL_REQUIRED_if_PIN_if_specified_in_session_configuration_but_not_SSL(void)
{
    const char* testPin = "192736aHUx@*G!Q";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = false,
            .port = 1234,
            .pin = ByteArray_Create(session.config.pinData, sizeof(session.config.pinData))
        }
    };
    strcpy((char*)session.config.pinData, testPin);

    KineticStatus status = KineticAuth_Populate(&session, &PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);

    TEST_IGNORE_MESSAGE("TODO: validate protobuf authentication fields");
}

void test_KineticAuth_Populate_should_return_AUTH_INFO_MISSING_if_neither_HMAC_key_nor_PIN_specified_in_the_session_config(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = 1234,
        }
    };

    KineticStatus status = KineticAuth_Populate(&session, &PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINTEIC_STATUS_AUTH_INFO_MISSING, status);
}



void test_KineticAuth_Populate_should_add_and_populate_PIN_authentication_if_PIN_and_SSL_specified_in_session_configuration(void)
{
    const char* testPin = "192736aHUx@*G!Q";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = true,
            .port = 1234,
            .pin = ByteArray_Create(session.config.pinData, sizeof(session.config.pinData))
        }
    };
    strcpy((char*)session.config.pinData, testPin);

    KineticStatus status = KineticAuth_Populate(&session, &PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_IGNORE_MESSAGE("TODO: validate protobuf authentication fields");
}

void test_KineticAuth_Populate_should_add_and_populate_PIN_if_specified_and_HMAC_specified_as_well_in_session_configuration(void)
{
    TEST_IGNORE_MESSAGE("Implement me!");
}


void test_KineticAuth_Populate_should_add_and_populate_HMAC_if_PIN_not_specified_in_session_configuration(void)
{
    const char* hmacKey = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = 1234,
            .hmacKey = ByteArray_Create(session.config.keyData, strlen(hmacKey)),
            .identity = 1,
        }
    };
    strcpy((char*)session.config.keyData, hmacKey);

    KineticStatus status = KineticAuth_Populate(&session, &PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_EQUAL_PTR(&PDU.protoData.message.hmacAuth, PDU.protoData.message.message.hmacAuth);
    TEST_ASSERT_NULL(PDU.protoData.message.message.pinAuth);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH, PDU.protoData.message.message.authType);
    TEST_ASSERT_TRUE(PDU.protoData.message.message.has_authType);
    TEST_ASSERT_EQUAL_PTR(PDU.hmac.data, PDU.protoData.message.hmacAuth.hmac.data);
    TEST_ASSERT_EQUAL(PDU.hmac.len, PDU.protoData.message.hmacAuth.hmac.len);
    TEST_ASSERT_TRUE(PDU.protoData.message.hmacAuth.has_identity);
    TEST_ASSERT_EQUAL(1, PDU.protoData.message.hmacAuth.identity);

    TEST_IGNORE_MESSAGE("TODO: validate protobuf authentication fields");
}
