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

#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic_logger.h"
#include <string.h>
#include <openssl/hmac.h>

static void KineticHMAC_Compute(KineticHMAC* hmac,
                                const KineticProto_Message* proto,
                                const ByteArray key);

void KineticHMAC_Init(KineticHMAC* hmac,
                      KineticProto_Command_Security_ACL_HMACAlgorithm algorithm)
{
    if (algorithm == KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1) {
        *hmac = (KineticHMAC) {
            .algorithm = algorithm,
             .len = KINETIC_HMAC_MAX_LEN
        };
    }
    else {
        *hmac = (KineticHMAC) {
            .algorithm = KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_INVALID_HMAC_ALGORITHM
        };
    }
}

void KineticHMAC_Populate(KineticHMAC* hmac,
                          KineticProto_Message* msg,
                          const ByteArray key)
{
    assert(hmac != NULL);
    assert(msg != NULL);
    assert(key.data != NULL);
    assert(key.len > 0);

    KineticHMAC_Init(hmac, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Compute(hmac, msg, key);

    // Copy computed HMAC into message
    memcpy(msg->hmacAuth->hmac.data, hmac->data, hmac->len);
    msg->hmacAuth->hmac.len = hmac->len;
    msg->hmacAuth->has_hmac = true;
}

bool KineticHMAC_Validate(const KineticProto_Message* msg,
                          const ByteArray key)
{
    assert(msg != NULL);
    assert(key.data != NULL);
    assert(key.len > 0);

    bool success = false;
    size_t i;
    int result = 0;
    KineticHMAC tempHMAC;

    if (!msg->has_authType
     || msg->authType != KINETIC_PROTO_MESSAGE_AUTH_TYPE_HMACAUTH
     || msg->hmacAuth == NULL
     || !msg->hmacAuth->has_hmac
     || msg->hmacAuth->hmac.data == NULL
     || msg->hmacAuth->hmac.len == 0) {
        return false;
    }

    KineticHMAC_Init(&tempHMAC, KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1);
    KineticHMAC_Compute(&tempHMAC, msg, key);
    if (msg->hmacAuth->hmac.len == tempHMAC.len) {
        for (i = 0; i < tempHMAC.len; i++) {
            result |= msg->hmacAuth->hmac.data[i] ^ tempHMAC.data[i];
        }
        success = (result == 0);
    }

    if (!success) {
        LOG("HMAC did not compare!");
        ByteArray expected = {.data = msg->hmacAuth->hmac.data, .len = msg->hmacAuth->hmac.len};
        KineticLogger_LogByteArray("expected HMAC", expected);
        ByteArray actual = {.data = tempHMAC.data, .len = tempHMAC.len};
        KineticLogger_LogByteArray("actual HMAC", actual);
    }

    return success;
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void KineticHMAC_Compute(KineticHMAC* hmac,
                                const KineticProto_Message* msg,
                                const ByteArray key)
{
    assert(hmac != NULL);
    assert(hmac->data != NULL);
    assert(hmac->len > 0);
    assert(msg != NULL);
    assert(msg->has_commandBytes);
    assert(msg->commandBytes.data != NULL);
    assert(msg->commandBytes.len > 0);

    uint32_t lenNBO = KineticNBO_FromHostU32(msg->commandBytes.len);

    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, key.data, key.len, EVP_sha1(), NULL);
    HMAC_Update(&ctx, (uint8_t*)&lenNBO, sizeof(uint32_t));
    HMAC_Update(&ctx, msg->commandBytes.data, msg->commandBytes.len);
    HMAC_Final(&ctx, hmac->data, &hmac->len);
    HMAC_CTX_cleanup(&ctx);
}
