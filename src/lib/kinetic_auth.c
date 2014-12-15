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

// #define KINETIC_MESSAGE_AUTH_HMAC_INIT(_msg, _identity, _hmac) { \
//     assert((_msg) != NULL); \
//     (_msg)->message.has_authType = true; \
//     (_msg)->message.authType = KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH; \
//     KineticProto_Message_hmacauth__init(&(_msg)->hmacAuth); \
//     (_msg)->message.hmacAuth = &(_msg)->hmacAuth; \
//     KineticProto_Message_pinauth__init(&(_msg)->pinAuth); \
//     (_msg)->message.pinAuth = NULL; \
//     (_msg)->command.header = &(_msg)->header; \
//     memset((_msg)->hmacData, 0, KINETIC_HMAC_MAX_LEN); \
//     if ((_hmac).len <= KINETIC_HMAC_MAX_LEN \
//         && (_hmac).data != NULL && (_hmac).len > 0 \
//         && (_msg)->hmacData != NULL) { \
//         memcpy((_msg)->hmacData, (_hmac).data, (_hmac).len);} \
//     (_msg)->message.hmacAuth->has_identity = true; \
//     (_msg)->message.hmacAuth->identity = (_identity); \
//     (_msg)->message.hmacAuth->has_hmac = true; \
//     (_msg)->message.hmacAuth->hmac = (ProtobufCBinaryData) { \
//         .data = (_msg)->hmacData, .len = SHA_DIGEST_LENGTH}; \
// }

KineticStatus KineticAuth_Populate(KineticSession const * const session, KineticPDU * const pdu)
{
    assert(session);
    assert(pdu);

    return KINETIC_STATUS_INVALID;
}
