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

#include "kinetic_hmac.h"
#include "kinetic_nbo.h"
#include "kinetic_logger.h"
#include <string.h>
#include <openssl/hmac.h>

static void KineticHMAC_Compute(KineticHMAC* hmac,
                                const Com__Seagate__Kinetic__Proto__Message* proto,
                                const ByteArray key);

void KineticHMAC_Init(KineticHMAC* hmac,
                      Com__Seagate__Kinetic__Proto__Command__Security__ACL__HMACAlgorithm algorithm)
{
    if (algorithm == COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1) {
        *hmac = (KineticHMAC) {
            .algorithm = algorithm,
            .len = KINETIC_HMAC_MAX_LEN
        };
    }
    else {
        *hmac = (KineticHMAC) {
            .algorithm = COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__INVALID_HMAC_ALGORITHM
        };
    }
}

void KineticHMAC_Populate(KineticHMAC* hmac,
                          Com__Seagate__Kinetic__Proto__Message* msg,
                          const ByteArray key)
{
    KINETIC_ASSERT(hmac != NULL);
    KINETIC_ASSERT(hmac->data != NULL);
    KINETIC_ASSERT(msg != NULL);
    KINETIC_ASSERT(key.data != NULL);
    KINETIC_ASSERT(key.len > 0);
    KINETIC_ASSERT(msg->hmacauth->hmac.data != NULL);

    KineticHMAC_Init(hmac, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Compute(hmac, msg, key);

    // Copy computed HMAC into message
    memcpy(msg->hmacauth->hmac.data, hmac->data, hmac->len);
    msg->hmacauth->hmac.len = hmac->len;
    msg->hmacauth->has_hmac = true;
}

bool KineticHMAC_Validate(const Com__Seagate__Kinetic__Proto__Message* msg,
                          const ByteArray key)
{
    KINETIC_ASSERT(msg != NULL);
    KINETIC_ASSERT(key.data != NULL);
    KINETIC_ASSERT(key.len > 0);

    bool success = false;
    size_t i;
    int result = 0;
    KineticHMAC tempHMAC;

    if (!msg->has_authtype
     || msg->authtype != COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__HMACAUTH
     || msg->hmacauth == NULL
     || !msg->hmacauth->has_hmac
     || msg->hmacauth->hmac.data == NULL
     || msg->hmacauth->hmac.len == 0) {
        return false;
    }

    KineticHMAC_Init(&tempHMAC, COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1);
    KineticHMAC_Compute(&tempHMAC, msg, key);
    if (msg->hmacauth->hmac.len == tempHMAC.len) {
        for (i = 0; i < tempHMAC.len; i++) {
            result |= msg->hmacauth->hmac.data[i] ^ tempHMAC.data[i];
        }
        success = (result == 0);
    }

    if (!success) {
        LOG0("HMAC did not compare!");
        ByteArray expected = {.data = msg->hmacauth->hmac.data, .len = msg->hmacauth->hmac.len};
        KineticLogger_LogByteArray(1, "expected HMAC", expected);
        ByteArray actual = {.data = tempHMAC.data, .len = tempHMAC.len};
        KineticLogger_LogByteArray(1, "actual HMAC", actual);
    }

    return success;
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define LOG_HMAC 0

static void KineticHMAC_Compute(KineticHMAC* hmac,
                                const Com__Seagate__Kinetic__Proto__Message* msg,
                                const ByteArray key)
{
    KINETIC_ASSERT(hmac != NULL);
    KINETIC_ASSERT(hmac->data != NULL);
    KINETIC_ASSERT(hmac->len > 0);
    KINETIC_ASSERT(msg != NULL);
    KINETIC_ASSERT(msg->has_commandbytes);
    KINETIC_ASSERT(msg->commandbytes.data != NULL);
    KINETIC_ASSERT(msg->commandbytes.len > 0);

    uint32_t lenNBO = KineticNBO_FromHostU32(msg->commandbytes.len);

    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);
#if LOG_HMAC
    fprintf(stderr, "\n\nUsing hmac key [%zd]: '%s' and length '", key.len, key.data);
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        fprintf(stderr, "%02x", ((uint8_t *)&lenNBO)[i]);
    }

    fprintf(stderr, "' on data: \n");
    for (size_t i = 0; i < msg->commandbytes.len; i++) {
        fprintf(stderr, "%02x", msg->commandbytes.data[i]);
        if (i > 0 && (i & 15) == 15) { fprintf(stderr, "\n"); }
    }
    fprintf(stderr, "\n\n");
#endif

    HMAC_Init_ex(&ctx, key.data, key.len, EVP_sha1(), NULL);
    HMAC_Update(&ctx, (uint8_t*)&lenNBO, sizeof(uint32_t));
    HMAC_Update(&ctx, msg->commandbytes.data, msg->commandbytes.len);
    HMAC_Final(&ctx, hmac->data, &hmac->len);
    HMAC_CTX_cleanup(&ctx);
}
