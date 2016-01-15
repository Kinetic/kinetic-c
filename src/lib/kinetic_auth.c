/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include "kinetic_auth.h"
#include "kinetic_hmac.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"

KineticStatus KineticAuth_EnsureSslEnabled(KineticSessionConfig const * const config)
{
    KINETIC_ASSERT(config);
    if (!config->useSsl) {return KINETIC_STATUS_SSL_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulateHmac(KineticSessionConfig const * const config, KineticRequest * const pdu)
{
    KINETIC_ASSERT(config);
    KINETIC_ASSERT(pdu);

    LOG3("Adding HMAC auth info");

    if (config->hmacKey.data == NULL) { return KINETIC_STATUS_HMAC_REQUIRED; }

    Com__Seagate__Kinetic__Proto__Message* msg = &pdu->message.message;

    // Add HMAC authentication struct
    msg->hmacauth = &pdu->message.hmacAuth;
    com__seagate__kinetic__proto__message__hmacauth__init(msg->hmacauth);
    msg->hmacauth = msg->hmacauth;
    msg->pinauth = NULL;
    msg->authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH;
    msg->has_authtype = true;

    // Configure HMAC support
    ByteArray const * const hmacKey = &config->hmacKey;
    KINETIC_ASSERT(hmacKey->len <= KINETIC_HMAC_MAX_LEN);  // NOCOMMIT
    KINETIC_ASSERT(hmacKey->data != NULL);

    msg->hmacauth = &pdu->message.hmacAuth;

    msg->hmacauth->hmac = (ProtobufCBinaryData) {
        .data = pdu->message.hmacData,
        .len = KINETIC_HMAC_SHA1_LEN,
    };

    msg->hmacauth->has_hmac = true;
    msg->hmacauth->identity = config->identity;
    msg->hmacauth->has_identity = true;

    // Populate with hashed HMAC
    KineticHMAC hmac;
    KineticHMAC_Init(&hmac, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Populate(&hmac, &pdu->message.message, config->hmacKey);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulatePin(KineticSessionConfig const * const config, KineticRequest * const pdu, ByteArray pin)
{
    KINETIC_ASSERT(config);
    KINETIC_ASSERT(pdu);

    LOG3("Adding PIN auth info");

    if (!config->useSsl) { return KINETIC_STATUS_SSL_REQUIRED; }

    KineticMessage* msg = &pdu->message;

    // Add PIN authentication struct
    com__seagate__kinetic__proto__message__pinauth__init(&msg->pinAuth);
    msg->message.pinauth = &msg->pinAuth;
    msg->message.hmacauth = NULL;
    msg->message.authtype = COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__PINAUTH;
    msg->message.has_authtype = true;
    msg->command.header = &msg->header;

    // Configure PIN support
    KINETIC_ASSERT(pin.len <= KINETIC_PIN_MAX_LEN);
    if (pin.len > 0) { KINETIC_ASSERT(pin.data != NULL); }
    msg->message.pinauth->pin = (ProtobufCBinaryData) {
        .data = pin.data,
        .len = pin.len,
    };
    msg->message.pinauth->has_pin = true;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulateTag(ByteBuffer * const tag, KineticAlgorithm algorithm, ByteArray const * const key)
{
    (void)tag;
    (void)algorithm;
    (void)key;
    return KINETIC_STATUS_INVALID;
}
