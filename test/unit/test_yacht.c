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
#include "yacht.h"

typedef struct yacht yacht;

void setUp(void) {}
void tearDown(void) {}

void test_yacht_should_cleanly_init_and_free(void) {
    yacht *y = yacht_init(8);
    TEST_ASSERT(y);
    yacht_free(y, NULL, NULL);
}


void yacht_should_add_and_have_accurate_membership(void) {
    uint8_t sz2 = 8;
    yacht *y = yacht_init(sz2);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        TEST_ASSERT(yacht_set(y, i, NULL, NULL));
        TEST_ASSERT(yacht_member(y, i));
        TEST_ASSERT(!yacht_member(y, i + 1));
    }

    yacht_free(y, NULL, NULL);
}

bool yacht_should_add_and_remove_accurately(uint8_t sz2) {
    yacht *y = yacht_init(sz2);
    TEST_ASSERT(y);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        uintptr_t v = 1;
        void *old = NULL;
        TEST_ASSERT(yacht_set(y, i, (void *)v, &old));
        TEST_ASSERT_EQUAL(NULL, old);
        TEST_ASSERT(yacht_member(y, i));
        void *old2 = NULL;
        TEST_ASSERT(yacht_remove(y, i, &old2));
        TEST_ASSERT_EQUAL((void *)1, old2);
        TEST_ASSERT(!yacht_member(y, i));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(yacht_set(y, i, NULL, NULL));
        TEST_ASSERT(yacht_member(y, i));

        for (int j = 0; j < i; j++) {
            TEST_ASSERT(yacht_member(y, j));
        }
    }

    yacht_free(y, NULL, NULL);
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
    yacht *y = yacht_init(sz2);
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        TEST_ASSERT(yacht_set(y, iv, NULL, NULL));
        TEST_ASSERT(yacht_member(y, iv));
        TEST_ASSERT(yacht_remove(y, iv, NULL));
        TEST_ASSERT(!yacht_member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(yacht_set(y, iv, NULL, NULL));
        TEST_ASSERT(yacht_member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
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

void yacht_set_should_return_old_keys(uint8_t sz2) {
    yacht *y = yacht_init(sz2);
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        uintptr_t old = 0;
        TEST_ASSERT(yacht_set(y, iv, (void *)1, (void *)&old));
        TEST_ASSERT_EQUAL(0, old);
        TEST_ASSERT(yacht_set(y, iv, (void *)2, (void *)&old));
        TEST_ASSERT_EQUAL(1, old);
        uintptr_t val = 0;
        TEST_ASSERT(yacht_get(y, iv, (void *)&val));
        TEST_ASSERT_EQUAL(2, val);

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
}

void test_yacht_set_should_return_old_keys8(void) {
    yacht_set_should_return_old_keys(8);
}

void test_yacht_set_should_return_old_keys10(void) {
    yacht_set_should_return_old_keys(10);
}

void test_yacht_set_should_return_old_keys14(void) {
    yacht_set_should_return_old_keys(14);
}

void yacht_should_grow_and_add_and_remove_accurately_out_of_order(uint8_t sz2) {
    yacht *y = yacht_init(0);   /* start at default small size */
    TEST_ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        uintptr_t v = 1;
        void *old = NULL;
        TEST_ASSERT(yacht_set(y, iv, (void *)v, &old));
        TEST_ASSERT_EQUAL(NULL, old);

        TEST_ASSERT(yacht_member(y, iv));

        void *old2 = NULL;
        TEST_ASSERT(yacht_remove(y, iv, &old2));
        TEST_ASSERT_EQUAL((void*)v, old2);

        TEST_ASSERT(!yacht_member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        TEST_ASSERT(yacht_set(y, iv, NULL, NULL));
        TEST_ASSERT(yacht_member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            TEST_ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
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
