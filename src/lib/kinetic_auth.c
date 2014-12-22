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

KineticStatus KineticAuth_EnsurePinSupplied(KineticSessionConfig const * const config)
{
    assert(config);
    if (config->pin.data == NULL) {return KINETIC_STATUS_PIN_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_EnsureSslEnabled(KineticSessionConfig const * const config)
{
    assert(config);
    if (!config->useSsl) {return KINETIC_STATUS_SSL_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

void auth_add_pin(KineticSessionConfig const * const config, KineticPDU * const pdu)
{
    LOG3("Adding PIN auth info");
    KineticMessage* msg = &pdu->protoData.message;

    // Add PIN authentication struct
    KineticProto_Message_pinauth__init(&msg->pinAuth);
    msg->message.pinAuth = &msg->pinAuth;
    msg->message.hmacAuth = NULL;
    msg->message.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_PINAUTH;
    msg->message.has_authType = true;
    msg->command.header = &msg->header;
    
    // Configure PIN support
    ByteArray const * const pin = &config->pin;
    assert(pin->len <= KINETIC_PIN_MAX_LEN);
    assert(pin->data != NULL);
    msg->message.pinAuth->pin = (ProtobufCBinaryData) {
        .data = config->pin.data,
        .len = config->pin.len,
    };
    msg->message.pinAuth->has_pin = true;
}

void auth_add_hmac(KineticSessionConfig const * const config, KineticPDU * const pdu)
{
    LOG3("Adding HMAC auth info");
    KineticProto_Message* msg = &pdu->protoData.message.message;

    // Add HMAC authentication struct
    msg->hmacAuth = &pdu->protoData.message.hmacAuth;
    KineticProto_Message_hmacauth__init(msg->hmacAuth);
    msg->hmacAuth = msg->hmacAuth;
    msg->pinAuth = NULL;
    msg->authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH;
    msg->has_authType = true;

    // Configure HMAC support
    ByteArray const * const hmac = &config->hmacKey;
    assert(hmac->len <= KINETIC_HMAC_MAX_LEN);
    assert(hmac->data != NULL);
    msg->hmacAuth = &pdu->protoData.message.hmacAuth;
    msg->hmacAuth->hmac = (ProtobufCBinaryData) {
        .data = pdu->hmac.data,
        .len = pdu->hmac.len,
    };
    msg->hmacAuth->has_hmac = true;
    msg->hmacAuth->identity = config->identity;
    msg->hmacAuth->has_identity = true;

    // Populate with hashed HMAC
    KineticHMAC_Init(&pdu->hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Populate(&pdu->hmac, pdu->proto, config->hmacKey);
}

KineticStatus KineticAuth_Populate(KineticSessionConfig const * const config, KineticPDU * const pdu)
{
    assert(config);
    assert(pdu);

    if (pdu->pinOp) {
        if (config->pin.data == NULL) {
            return KINETIC_STATUS_PIN_REQUIRED;
        }
        if (!config->useSsl) {
            return KINETIC_STATUS_SSL_REQUIRED;
        }
        auth_add_pin(config, pdu);
    }
    else
    {
        if (config->hmacKey.data == NULL) {
            return KINETIC_STATUS_HMAC_REQUIRED;
        }
        auth_add_hmac(config, pdu);
    }

    return KINETIC_STATUS_SUCCESS;
}
