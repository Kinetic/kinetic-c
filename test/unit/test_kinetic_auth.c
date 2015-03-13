/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

KineticSession Session;
KineticRequest Request;

void setUp(void)
{
    Session = (KineticSession) {
        .config = (KineticSessionConfig) {.host = "anyhost", .port = KINETIC_PORT}
    };
    KineticLogger_Init("stdout", 3);
    KineticRequest_Init(&Request, &Session);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticAuth_EnsureSslEnabled_should_return_SUCCESS_if_SSL_is_enabled(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = true,
            .port = 1234
        }
    };

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, KineticAuth_EnsureSslEnabled(&session.config));
}

void test_KineticAuth_EnsureSslEnabled_should_return_SSL_REQUIRED_if_SSL_not_enabled(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = 1234
        }
    };

    KineticStatus status = KineticAuth_EnsureSslEnabled(&session.config);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SSL_REQUIRED, status);
}



void test_KineticAuth_PopulateHmac_should_return_HMAC_REQUIRED_if_HMAC_not_specified_in_the_session_config(void)
{
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = 1234,
        }
    };

    KineticStatus status = KineticAuth_PopulateHmac(&session.config, &Request);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticAuth_PopulateHmac_should_add_and_populate_HMAC_authentication(void)
{
    KineticRequest_Init(&Request, &Session);
    Request.message.message.has_commandBytes = true;
    uint8_t dummyMessageBytes[16];
    Request.message.message.commandBytes = (ProtobufCBinaryData) {
        .data = dummyMessageBytes,
        .len = sizeof(dummyMessageBytes),
    };

    const char* hmacKey = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = 1234,
            .hmacKey = ByteArray_Create(session.config.keyData, strlen(hmacKey)),
            .identity = 1,
        }
    };
    strcpy((char*)session.config.keyData, hmacKey);

    KineticStatus status = KineticAuth_PopulateHmac(&session.config, &Request);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_NULL(Request.message.message.pinAuth);
    TEST_ASSERT_TRUE(Request.message.message.has_authType);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH, Request.message.message.authType);
    TEST_ASSERT_TRUE(Request.message.message.hmacAuth->has_hmac);
    TEST_ASSERT_EQUAL_PTR(Request.message.hmacAuth.hmac.data, Request.message.message.hmacAuth->hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_SHA1_LEN, Request.message.message.hmacAuth->hmac.len);
    TEST_ASSERT_EQUAL_PTR(Request.message.hmacData, Request.message.hmacAuth.hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_SHA1_LEN, Request.message.hmacAuth.hmac.len);
    TEST_ASSERT_TRUE(Request.message.hmacAuth.has_identity);
    TEST_ASSERT_EQUAL(1, Request.message.hmacAuth.identity);
}


void test_KineticAuth_Populate_should_add_and_populate_PIN_authentication(void)
{
    char testPin[] = "192736aHUx@*G!Q";
    ByteArray pin = ByteArray_Create(testPin, strlen(testPin));
    LOGF0("pin data=%p, len=%zu", pin.data, pin.len);

    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = true,
            .port = 1234,
        }
    };

    KineticStatus status = KineticAuth_PopulatePin(&session.config, &Request, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_NULL(Request.message.message.hmacAuth);
    TEST_ASSERT_TRUE(Request.message.message.has_authType);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__PINAUTH, Request.message.message.authType);
    TEST_ASSERT_TRUE(Request.message.message.pinAuth->has_pin);
    TEST_ASSERT_EQUAL_PTR(testPin, Request.message.message.pinAuth->pin.data);
    TEST_ASSERT_EQUAL(strlen(testPin), Request.message.message.pinAuth->pin.len);
}
