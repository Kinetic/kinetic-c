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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_types.h"
#include "kinetic_hmac.h"
#include <string.h>
#include <arpa/inet.h>
#include <openssl/hmac.h>

static void KineticHMAC_Compute(KineticHMAC* hmac, const KineticProto* proto, const char* const key, size_t keyLen);

void KineticHMAC_Init(KineticHMAC * hmac, KineticProto_Security_ACL_HMACAlgorithm algorithm)
{
    if (algorithm == KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_HmacSHA1)
    {
        hmac->algorithm = algorithm;
        memset(hmac->value, 0, KINETIC_HMAC_MAX_LEN);
        hmac->valueLength = 0;
    }
    else
    {
        hmac->algorithm = KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_INVALID_HMAC_ALGORITHM;
    }
}

void KineticHMAC_Populate(
    KineticHMAC* hmac,
    KineticMessage* message,
    const char* const key,
    size_t keyLen)
{
    KineticHMAC_Init(hmac, hmac->algorithm);
    KineticHMAC_Compute(hmac, &message->proto, key, keyLen);

    // Copy computed HMAC into message
    memcpy(message->proto.hmac.data, hmac->value, hmac->valueLength);
    message->proto.hmac.len = hmac->valueLength;
    message->proto.has_hmac = true;
}

bool KineticHMAC_Validate(
    const KineticProto* proto,
    const char* const key,
    size_t keyLen)
{
    size_t i;
    int result = 0;
    KineticHMAC tempHMAC;

    if (!proto->has_hmac)
    {
        return false;
    }

    KineticHMAC_Init(&tempHMAC, KINETIC_PROTO_SECURITY_ACL_HMACALGORITHM_INVALID_HMAC_ALGORITHM);
    KineticHMAC_Compute(&tempHMAC, proto, key, keyLen);

    if (proto->hmac.len != tempHMAC.valueLength)
    {
        return false;
    }

    for (i = 0; i < tempHMAC.valueLength; i++)
    {
        result |= proto->hmac.data[i] ^ tempHMAC.value[i];
    }

    return (result == 0);
}

static void KineticHMAC_Compute(
    KineticHMAC* hmac,
    const KineticProto* proto,
    const char* const key,
    size_t keyLen)
{
    HMAC_CTX ctx;

    unsigned int len = protobuf_c_message_get_packed_size((ProtobufCMessage*)proto->command);
    uint8_t* command = malloc(len);
    protobuf_c_message_pack((ProtobufCMessage*)proto->command, command);

#pragma push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, key, keyLen, EVP_sha1(), NULL);

    if (keyLen != 0)
    {
        uint32_t messageLengthNBO = htonl(keyLen);
        HMAC_Update(&ctx, (uint8_t*)(&messageLengthNBO), sizeof(uint32_t));
        HMAC_Update(&ctx, command, len);
    }

    HMAC_Final(&ctx, hmac->value, &hmac->valueLength);
    HMAC_CTX_cleanup(&ctx);

#pragma pop

    free(command);
}
