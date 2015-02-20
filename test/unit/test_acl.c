/*
* kinetic-c-client
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
#include "acl.h"
#include <string.h>
#include <stdio.h>

#define TEST_DIR(F) ("test/unit/acl/" F)

void test_acl_of_empty_JSON_object_should_fail(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex0.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_MISSING_FIELD, res);
}

void test_acl_of_nonexistent_file_should_fail(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("nonexistent.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_BAD_JSON, res);
}

void test_acl_of_file_ex1_should_parse_file_as_expected(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex1.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    TEST_ASSERT_EQUAL(1, acl->identity);

    TEST_ASSERT_EQUAL(HMAC_SHA1, acl->hmacKey->type);
    TEST_ASSERT_EQUAL(64, acl->hmacKey->length);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->hmacKey->key,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(2, acl->scopeCount);

    TEST_ASSERT_EQUAL(1, acl->scopes[0].permission_count);
    TEST_ASSERT_EQUAL(PERM_READ, acl->scopes[0].permissions[0]);

    TEST_ASSERT_EQUAL(0, acl->scopes[1].offset);
    TEST_ASSERT_EQUAL(1, acl->scopes[1].permission_count);
    TEST_ASSERT_EQUAL(PERM_WRITE, acl->scopes[1].permissions[0]);
    TEST_ASSERT_EQUAL(3, acl->scopes[1].valueSize);
    TEST_ASSERT_EQUAL(0, strcmp((char *)acl->scopes[1].value, "foo"));

    acl_free(acl);
}

void test_acl_of_file_ex2_should_parse_file_as_expected(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex2.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    TEST_ASSERT_EQUAL(2, acl->identity);

    TEST_ASSERT_EQUAL(HMAC_SHA1, acl->hmacKey->type);
    TEST_ASSERT_EQUAL(64, acl->hmacKey->length);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->hmacKey->key,
            "13010b8d8acdbe6abc005840aad1dc5dedb4345e681ed4e3c4645d891241d6b2"));

    TEST_ASSERT_EQUAL(1, acl->scopeCount);
    TEST_ASSERT_EQUAL(1, acl->scopes[0].permission_count);
    TEST_ASSERT_EQUAL(PERM_SECURITY, acl->scopes[0].permissions[0]);
    TEST_ASSERT_EQUAL(true, acl->scopes[0].tlsRequired);

    acl_free(acl);
}

void test_acl_of_file_should_reject_bad_HMAC_type(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex_bad_hmac.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_INVALID_FIELD, res);
}

void test_acl_of_file_should_recognize_all_permission_types(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex3.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    TEST_ASSERT_EQUAL(3, acl->identity);

    TEST_ASSERT_EQUAL(HMAC_SHA1, acl->hmacKey->type);
    TEST_ASSERT_EQUAL(64, acl->hmacKey->length);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->hmacKey->key,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(8, acl->scopeCount);

    for (size_t i = 0; i < acl->scopeCount; i++) {
        TEST_ASSERT_EQUAL(1, acl->scopes[i].permission_count);
    }
    TEST_ASSERT_EQUAL(PERM_READ, acl->scopes[0].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_WRITE, acl->scopes[1].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_DELETE, acl->scopes[2].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_RANGE, acl->scopes[3].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_SETUP, acl->scopes[4].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_P2POP, acl->scopes[5].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_GETLOG, acl->scopes[6].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_SECURITY, acl->scopes[7].permissions[0]);

    acl_free(acl);
}

void test_acl_of_file_should_handle_multiple_permissions(void)
{
    struct ACL *acl = NULL;
    acl_of_file_res res = acl_of_file(TEST_DIR("ex-multi-permission.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_OK, res);
    
    TEST_ASSERT_EQUAL(3, acl->scopes[0].permission_count);
    TEST_ASSERT_EQUAL(PERM_READ, acl->scopes[0].permissions[0]);
    TEST_ASSERT_EQUAL(PERM_WRITE, acl->scopes[0].permissions[1]);
    TEST_ASSERT_EQUAL(PERM_SETUP, acl->scopes[0].permissions[2]);

    acl_free(acl);
}
