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
#include "kinetic_proto.h"
#include "kinetic_logger.h"

KineticStatus KineticAuth_EnsurePinSupplied(KineticSession const * const session)
{
    assert(session);
    if (session->config.pin.data == NULL) {return KINETIC_STATUS_PIN_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticAuth_EnsureSslEnabled(KineticSession const * const session)
{
    assert(session);
    if (!session->config.useSsl) {return KINETIC_STATUS_SSL_REQUIRED;}
    return KINETIC_STATUS_SUCCESS;
}

void auth_add_pin(KineticSession const * const session, KineticPDU * const pdu)
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
    ByteArray const * const pin = &session->config.pin;
    assert(pin->len <= KINETIC_PIN_MAX_LEN);
    assert(pin->data != NULL);
    msg->message.pinAuth->pin = (ProtobufCBinaryData) {
        .data = session->config.pin.data,
        .len = session->config.pin.len,
    };
    msg->message.pinAuth->has_pin = true;
}

void auth_add_hmac(KineticSession const * const session, KineticPDU * const pdu)
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
    ByteArray const * const hmac = &session->config.hmacKey;
    assert(hmac->len <= KINETIC_HMAC_MAX_LEN);
    assert(hmac->data != NULL);
    msg->hmacAuth = &pdu->protoData.message.hmacAuth;
    msg->hmacAuth->hmac = (ProtobufCBinaryData) {
        .data = pdu->hmac.data,
        .len = pdu->hmac.len,
    };
    msg->hmacAuth->has_hmac = true;
    msg->hmacAuth->identity = session->config.identity;
    msg->hmacAuth->has_identity = true;
}

KineticStatus KineticAuth_Populate(KineticSession const * const session, KineticPDU * const pdu)
{
    assert(session);
    assert(pdu);

    if ((session->config.pin.data == NULL) && (session->config.hmacKey.data == NULL)) {
        return KINTEIC_STATUS_AUTH_INFO_MISSING;
    }

    // PIN auth takes precedence over HMAC, if both specified
    if (session->config.pin.data != NULL) {
        if (!session->config.useSsl) {
            return KINETIC_STATUS_SSL_REQUIRED;
        }
        auth_add_pin(session, pdu);
    }
    else
    {
        auth_add_hmac(session, pdu);
    }

    return KINETIC_STATUS_SUCCESS;
}
