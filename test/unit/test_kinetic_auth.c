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
#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

KineticConnection Connection;
KineticSession Session;
KineticPDU PDU;

void setUp(void)
{
    KineticSessionConfig config = (KineticSessionConfig) {.host = "anyhost", .port = KINETIC_PORT};
    KineticSession_Init(&Session, &config, &Connection);
    KineticLogger_Init("stdout", 3);
    KineticPDU_InitWithCommand(&PDU, &Session);
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

    KineticStatus status = KineticAuth_PopulateHmac(&session.config, &PDU);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_HMAC_REQUIRED, status);
}

void test_KineticAuth_PopulateHmac_should_add_and_populate_HMAC_authentication(void)
{ LOG_LOCATION;
    KineticPDU_InitWithCommand(&PDU, &Session);
    PDU.message.message.has_commandBytes = true;
    uint8_t dummyMessageBytes[16];
    PDU.message.message.commandBytes = (ProtobufCBinaryData) {
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

    KineticStatus status = KineticAuth_PopulateHmac(&session.config, &PDU);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_NULL(PDU.message.message.pinAuth);
    TEST_ASSERT_TRUE(PDU.message.message.has_authType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH, PDU.message.message.authType);
    TEST_ASSERT_TRUE(PDU.message.message.hmacAuth->has_hmac);
    TEST_ASSERT_EQUAL_PTR(PDU.message.hmacAuth.hmac.data, PDU.message.message.hmacAuth->hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_SHA1_LEN, PDU.message.message.hmacAuth->hmac.len);
    TEST_ASSERT_EQUAL_PTR(PDU.message.hmacData, PDU.message.hmacAuth.hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_SHA1_LEN, PDU.message.hmacAuth.hmac.len);
    TEST_ASSERT_TRUE(PDU.message.hmacAuth.has_identity);
    TEST_ASSERT_EQUAL(1, PDU.message.hmacAuth.identity);
}


void test_KineticAuth_Populate_should_add_and_populate_PIN_authentication(void)
{ LOG_LOCATION;
    char testPin[] = "192736aHUx@*G!Q";
    ByteArray pin = ByteArray_Create(testPin, strlen(testPin));
    LOGF0("pin data=%p, len=%zu", pin.data, pin.len);

    KineticSession session = {
        .config = (KineticSessionConfig) {
            .useSsl = true,
            .port = 1234,
        }
    };

    KineticStatus status = KineticAuth_PopulatePin(&session.config, &PDU, pin);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    TEST_ASSERT_NULL(PDU.message.message.hmacAuth);
    TEST_ASSERT_TRUE(PDU.message.message.has_authType);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH, PDU.message.message.authType);
    TEST_ASSERT_TRUE(PDU.message.message.pinAuth->has_pin);
    TEST_ASSERT_EQUAL_PTR(testPin, PDU.message.message.pinAuth->pin.data);
    TEST_ASSERT_EQUAL(strlen(testPin), PDU.message.message.pinAuth->pin.len);
}
