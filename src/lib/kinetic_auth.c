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

#include "kinetic_auth.h"
#include "kinetic_hmac.h"
#include "kinetic_proto.h"
#include "kinetic_logger.h"

KineticStatus KineticAuth_EnsureSslEnabled(KineticSessionConfig const * const config)
{
    assert(config);
    if (!config->useSsl) {return KINETIC_STATUS_SSL_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulateHmac(KineticSessionConfig const * const config, KineticRequest * const pdu)
{
    assert(config);
    assert(pdu);

    LOG3("Adding HMAC auth info");

    if (config->hmacKey.data == NULL) { return KINETIC_STATUS_HMAC_REQUIRED; }

    KineticProto_Message* msg = &pdu->message.message;

    // Add HMAC authentication struct
    msg->hmacAuth = &pdu->message.hmacAuth;
    KineticProto_Message_hmacauth__init(msg->hmacAuth);
    msg->hmacAuth = msg->hmacAuth;
    msg->pinAuth = NULL;
    msg->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    msg->has_authType = true;

    // Configure HMAC support
    ByteArray const * const hmacKey = &config->hmacKey;
    assert(hmacKey->len <= KINETIC_HMAC_MAX_LEN);  // NOCOMMIT
    assert(hmacKey->data != NULL);

    msg->hmacAuth = &pdu->message.hmacAuth;

    msg->hmacAuth->hmac = (ProtobufCBinaryData) {
        .data = pdu->message.hmacData,
        .len = KINETIC_HMAC_SHA1_LEN,
    };

    msg->hmacAuth->has_hmac = true;
    msg->hmacAuth->identity = config->identity;
    msg->hmacAuth->has_identity = true;

    // Populate with hashed HMAC
    KineticHMAC hmac;
    KineticHMAC_Init(&hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&hmac, &pdu->message.message, config->hmacKey);

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulatePin(KineticSessionConfig const * const config, KineticRequest * const pdu, ByteArray pin)
{
    assert(config);
    assert(pdu);

    LOG3("Adding PIN auth info");

    if (!config->useSsl) { return KINETIC_STATUS_SSL_REQUIRED; }

    KineticMessage* msg = &pdu->message;

    // Add PIN authentication struct
    KineticProto_Message_pinauth__init(&msg->pinAuth);
    msg->message.pinAuth = &msg->pinAuth;
    msg->message.hmacAuth = NULL;
    msg->message.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH;
    msg->message.has_authType = true;
    msg->command.header = &msg->header;
    
    // Configure PIN support
    assert(pin.len <= KINETIC_PIN_MAX_LEN);
    if (pin.len > 0) { assert(pin.data != NULL); }
    msg->message.pinAuth->pin = (ProtobufCBinaryData) {
        .data = pin.data,
        .len = pin.len,
    };
    msg->message.pinAuth->has_pin = true;

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_PopulateTag(ByteBuffer * const tag, KineticAlgorithm algorithm, ByteArray const * const key)
{
    (void)tag;
    (void)algorithm;
    (void)key;
    return KINETIC_STATUS_INVALID;
}
