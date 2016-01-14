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
#include "yacht.h"

typedef struct yacht yacht;

void setUp(void) {}
void tearDown(void) {}

void test_yacht_should_cleanly_init_and_free(void) {
    yacht *y = Yacht_Init(8);
    TEST_ASSERT(y);
    Yacht_Free(y, NULL, NULL);
}


void yacht_should_add_and_have_accurate_membership(void) {
    uint8_t sz2 = 8;
    yacht *y = Yacht_Init(sz2);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        TEST_ASSERT(Yacht_Set(y, i, NULL, NULL));
        TEST_ASSERT(Yacht_Member(y, i));
        TEST_ASSERT(!Yacht_Member(y, i + 1));
    }

    Yacht_Free(y, NULL, NULL);
}

bool yacht_should_add_and_remove_accurately(uint8_t sz2) {
    yacht *y = Yacht_Init(sz2);
    TEST_ASSERT(y);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        uintptr_t v = 1;
        void *old = NULL;
        TEST_ASSERT(Yacht_Set(y, i, (void *)v, &old));
        TEST_ASSERT_EQUAL(NULL, old);
        TEST_ASSERT(Yacht_Member(y, i));
        void *old2 = NULL;
        TEST_ASSERT(Yacht_Remove(y, i, &old2));
        TEST_ASSERT_EQUAL((void *)1, old2);
        TEST_ASSERT(!Yacht_Member(y, i));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(Yacht_Set(y, i, NULL, NULL));
        TEST_ASSERT(Yacht_Member(y, i));

        for (int j = 0; j < i; j++) {
            TEST_ASSERT(Yacht_Member(y, j));
        }
    }

    Yacht_Free(y, NULL, NULL);
    return true;
}

void test_yacht_should_add_and_remove_accurately8(void) {
    yacht_should_add_and_remove_accurately(8);
}

void test_yacht_should_add_and_remove_accurately10(void) {
    yacht_should_add_and_remove_accurately(10);
}

void test_yacht_should_add_and_remove_accurately14(void) {
    yacht_should_add_and_remove_accurately(14);
}

void yacht_should_add_and_remove_accurately_out_of_order(uint8_t sz2) {
    yacht *y = Yacht_Init(sz2);
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        TEST_ASSERT(Yacht_Set(y, iv, NULL, NULL));
        TEST_ASSERT(Yacht_Member(y, iv));
        TEST_ASSERT(Yacht_Remove(y, iv, NULL));
        TEST_ASSERT(!Yacht_Member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(Yacht_Set(y, iv, NULL, NULL));
        TEST_ASSERT(Yacht_Member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(Yacht_Member(y, jv));
        }
    }

    Yacht_Free(y, NULL, NULL);
}

void test_yacht_should_add_and_remove_accurately_out_of_order8(void) {
    yacht_should_add_and_remove_accurately_out_of_order(8);
}
void test_yacht_should_add_and_remove_accurately_out_of_order10(void) {
    yacht_should_add_and_remove_accurately_out_of_order(10);
}
void test_yacht_should_add_and_remove_accurately_out_of_order14(void) {
    yacht_should_add_and_remove_accurately_out_of_order(14);
}

void Yacht_Set_should_return_old_keys(uint8_t sz2) {
    yacht *y = Yacht_Init(sz2);
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        uintptr_t old = 0;
        TEST_ASSERT(Yacht_Set(y, iv, (void *)1, (void *)&old));
        TEST_ASSERT_EQUAL(0, old);
        TEST_ASSERT(Yacht_Set(y, iv, (void *)2, (void *)&old));
        TEST_ASSERT_EQUAL(1, old);
        uintptr_t val = 0;
        TEST_ASSERT(Yacht_Get(y, iv, (void *)&val));
        TEST_ASSERT_EQUAL(2, val);

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(Yacht_Member(y, jv));
        }
    }

    Yacht_Free(y, NULL, NULL);
}

void test_Yacht_Set_should_return_old_keys8(void) {
    Yacht_Set_should_return_old_keys(8);
}

void test_Yacht_Set_should_return_old_keys10(void) {
    Yacht_Set_should_return_old_keys(10);
}

void test_Yacht_Set_should_return_old_keys14(void) {
    Yacht_Set_should_return_old_keys(14);
}

void yacht_should_grow_and_add_and_remove_accurately_out_of_order(uint8_t sz2) {
    yacht *y = Yacht_Init(0);   /* start at default small size */
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        uintptr_t v = 1;
        void *old = NULL;
        TEST_ASSERT(Yacht_Set(y, iv, (void *)v, &old));
        TEST_ASSERT_EQUAL(NULL, old);

        TEST_ASSERT(Yacht_Member(y, iv));

        void *old2 = NULL;
        TEST_ASSERT(Yacht_Remove(y, iv, &old2));
        TEST_ASSERT_EQUAL((void*)v, old2);

        TEST_ASSERT(!Yacht_Member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(Yacht_Set(y, iv, NULL, NULL));
        TEST_ASSERT(Yacht_Member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(Yacht_Member(y, jv));
        }
    }

    Yacht_Free(y, NULL, NULL);
}

void test_yacht_should_grow_and_add_and_remove_accurately_out_of_order8(void)
{
    yacht_should_grow_and_add_and_remove_accurately_out_of_order(8);
}

void test_yacht_should_grow_and_add_and_remove_accurately_out_of_order10(void)
{
    yacht_should_grow_and_add_and_remove_accurately_out_of_order(10);
}

void test_yacht_should_grow_and_add_and_remove_accurately_out_of_order14(void)
{
    yacht_should_grow_and_add_and_remove_accurately_out_of_order(14);
}
