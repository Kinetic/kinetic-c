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

#include "unity.h"
#include "kinetic_acl.h"

#include <string.h>
#include <stdio.h>

#include "mock_kinetic_logger.h"

#include "kinetic.pb-c.h"
#include "protobuf-c.h"

#define TEST_DIR(F) ("test/unit/acl/" F)

void test_acl_of_empty_JSON_object_should_fail(void)
{
    struct ACL *acl = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex0.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_MISSING_FIELD, res);
}

void test_acl_of_nonexistent_file_should_fail(void)
{
    struct ACL *acl = NULL;
    const char *path = TEST_DIR("nonexistent.json");
    KineticACLLoadResult res = KineticACL_LoadFromFile(path, &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_BAD_JSON, res);
}

void test_KineticACL_LoadFromFile_ex1_should_parse_file_as_expected(void)
{
    struct ACL *acls = NULL;

    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex1.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    TEST_ASSERT_EQUAL(1, acls->ACL_count);
    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl = acls->ACLs[0];

    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(1, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacalgorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1,
        acl->hmacalgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(2, acl->n_scope);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL__Scope *sc0 = acl->scope[0];
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__READ,
        sc0->permission[0]);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL__Scope *sc1 = acl->scope[1];
    TEST_ASSERT_TRUE(sc1->has_offset);
    TEST_ASSERT_EQUAL(0, sc1->offset);
    TEST_ASSERT_EQUAL(1, sc1->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__WRITE,
        sc1->permission[0]);
    TEST_ASSERT_TRUE(sc1->has_value);
    TEST_ASSERT_EQUAL(3, sc1->value.len);
    TEST_ASSERT_EQUAL(0, strcmp((char *)sc1->value.data, "foo"));

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_ex2_should_parse_file_as_expected(void)
{
    struct ACL *acls = NULL;

    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex2.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);
    TEST_ASSERT_EQUAL(1, acls->ACL_count);
    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl = acls->ACLs[0];

    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(2, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacalgorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1,
        acl->hmacalgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "13010b8d8acdbe6abc005840aad1dc5dedb4345e681ed4e3c4645d891241d6b2"));

    Com__Seagate__Kinetic__Proto__Command__Security__ACL__Scope *sc0 = acl->scope[0];
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__SECURITY,
        sc0->permission[0]);
    TEST_ASSERT_EQUAL(true, sc0->tlsrequired);

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_should_reject_bad_HMAC_type(void)
{
    struct ACL *acl = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex_bad_hmac.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_INVALID_FIELD, res);
}

void test_KineticACL_LoadFromFile_should_recognize_all_permission_types(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex3.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl = acls->ACLs[0];
    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(3, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacalgorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1,
        acl->hmacalgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(8, acl->n_scope);

    for (size_t i = 0; i < acl->n_scope; i++) {
        TEST_ASSERT_EQUAL(1, acl->scope[i]->n_permission);
    }

    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__READ,
        acl->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__WRITE,
        acl->scope[1]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__DELETE,
        acl->scope[2]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__RANGE,
        acl->scope[3]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__SETUP,
        acl->scope[4]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__P2POP,
        acl->scope[5]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__GETLOG,
        acl->scope[6]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__SECURITY,
        acl->scope[7]->permission[0]);

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_should_handle_multiple_permissions(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex-multi-permission.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl = acls->ACLs[0];

    TEST_ASSERT_EQUAL(3, acl->scope[0]->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__READ, acl->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__WRITE, acl->scope[0]->permission[1]);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__SETUP, acl->scope[0]->permission[2]);

    KineticACL_Free(acls);
}

void test_acl_should_handle_multiple_JSON_objects(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex_multi.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl = acls->ACLs[0];
    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(1, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacalgorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1,
        acl->hmacalgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(2, acl->n_scope);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL__Scope *sc0 = acl->scope[0];
    TEST_ASSERT_FALSE(sc0->has_offset);
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__READ,
        sc0->permission[0]);

    Com__Seagate__Kinetic__Proto__Command__Security__ACL__Scope *sc1 = acl->scope[1];
    TEST_ASSERT_TRUE(sc1->has_offset);
    TEST_ASSERT_EQUAL(0, sc1->offset);
    TEST_ASSERT_EQUAL(1, sc1->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__WRITE,
        sc1->permission[0]);
    TEST_ASSERT_TRUE(sc1->has_value);
    TEST_ASSERT_EQUAL(3, sc1->value.len);
    TEST_ASSERT_EQUAL(0, strcmp((char *)sc1->value.data, "foo"));

    Com__Seagate__Kinetic__Proto__Command__Security__ACL *acl2 = acls->ACLs[1];
    TEST_ASSERT_NOT_NULL(acl2);

    TEST_ASSERT_TRUE(acl2->has_identity);
    TEST_ASSERT_EQUAL(2, acl2->identity);

    TEST_ASSERT_TRUE(acl2->has_hmacalgorithm);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__HMACALGORITHM__HmacSHA1,
        acl2->hmacalgorithm);

    TEST_ASSERT_TRUE(acl2->has_key);
    TEST_ASSERT_EQUAL(64, acl2->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl2->key.data,
            "13010b8d8acdbe6abc005840aad1dc5dedb4345e681ed4e3c4645d891241d6b2"));

    TEST_ASSERT_EQUAL(1, acl2->n_scope);
    TEST_ASSERT_EQUAL(1, acl2->scope[0]->n_permission);
    TEST_ASSERT_EQUAL(COM__SEAGATE__KINETIC__PROTO__COMMAND__SECURITY__ACL__PERMISSION__SECURITY,
        acl2->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(true, acl2->scope[0]->tlsrequired);

    KineticACL_Free(acls);
}
