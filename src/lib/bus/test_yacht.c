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
#include "greatest.h"
#include "yacht.h"

typedef struct yacht yacht;

TEST yacht_should_cleanly_init_and_free(void) {
    yacht *y = yacht_init(8);
    ASSERT(y);
    yacht_free(y, NULL, NULL);
    PASS();
}

TEST yacht_should_add_and_have_accurate_membership(void) {
    uint8_t sz2 = 8;
    yacht *y = yacht_init(sz2);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        ASSERT(yacht_set(y, i, NULL, NULL));
        ASSERT(yacht_member(y, i));
        ASSERT(!yacht_member(y, i + 1));
    }

    yacht_free(y, NULL, NULL);
    PASS();
}

TEST yacht_should_add_and_remove_accurately(uint8_t sz2) {
    yacht *y = yacht_init(sz2);
    ASSERT(y);

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        ASSERT(yacht_set(y, i, NULL, NULL));
        ASSERT(yacht_member(y, i));
        ASSERT(yacht_remove(y, i, NULL));
        ASSERT(!yacht_member(y, i));
        /* Add it back, to ensure it isn't disturbed by other removes */
        ASSERT(yacht_set(y, i, NULL, NULL));
        ASSERT(yacht_member(y, i));

        for (int j = 0; j < i; j++) {
            ASSERT(yacht_member(y, j));
        }
    }

    yacht_free(y, NULL, NULL);
    PASS();
}

TEST yacht_should_add_and_remove_accurately_out_of_order(uint8_t sz2) {
    yacht *y = yacht_init(sz2);
    ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        ASSERT(yacht_set(y, iv, NULL, NULL));
        ASSERT(yacht_member(y, iv));
        ASSERT(yacht_remove(y, iv, NULL));
        ASSERT(!yacht_member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        ASSERT(yacht_set(y, iv, NULL, NULL));
        ASSERT(yacht_member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
    PASS();
}

TEST yacht_set_should_return_old_keys(uint8_t sz2) {
    yacht *y = yacht_init(sz2);
    ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        uintptr_t old = 0;
        ASSERT(yacht_set(y, iv, (void *)1, (void *)&old));
        ASSERT_EQ(0, old);
        ASSERT(yacht_set(y, iv, (void *)2, (void *)&old));
        ASSERT_EQ(1, old);
        uintptr_t val = 0;
        ASSERT(yacht_get(y, iv, (void *)&val));
        ASSERT_EQ(2, val);

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
    PASS();
}

TEST yacht_should_grow_and_add_and_remove_accurately_out_of_order(uint8_t sz2) {
    yacht *y = yacht_init(0);   /* start at default small size */
    ASSERT(y);

    const uint64_t other_large_prime = 4294967279L; /* (2 ** 32) - 17 */

    for (int i = 0; i < (1 << (sz2 - 1)); i++) {
        int iv = i * other_large_prime;
        ASSERT(yacht_set(y, iv, NULL, NULL));
        ASSERT(yacht_member(y, iv));
        ASSERT(yacht_remove(y, iv, NULL));
        ASSERT(!yacht_member(y, iv));
        /* Add it back, to ensure it isn't disturbed by other removes */
        ASSERT(yacht_set(y, iv, NULL, NULL));
        ASSERT(yacht_member(y, iv));

        for (int j = 0; j < i; j++) {
            int jv = j * other_large_prime;
            ASSERT(yacht_member(y, jv));
        }
    }

    yacht_free(y, NULL, NULL);
    PASS();
}

SUITE(suite) {
    RUN_TEST(yacht_should_cleanly_init_and_free);
    RUN_TEST(yacht_should_add_and_have_accurate_membership);
    RUN_TESTp(yacht_should_add_and_remove_accurately, 8);
    RUN_TESTp(yacht_should_add_and_remove_accurately, 10);
    RUN_TESTp(yacht_should_add_and_remove_accurately, 14);
    RUN_TESTp(yacht_should_add_and_remove_accurately_out_of_order, 8);
    RUN_TESTp(yacht_should_add_and_remove_accurately_out_of_order, 10);
    RUN_TESTp(yacht_should_add_and_remove_accurately_out_of_order, 14);
    RUN_TESTp(yacht_set_should_return_old_keys, 8);
    RUN_TESTp(yacht_set_should_return_old_keys, 10);
    RUN_TESTp(yacht_set_should_return_old_keys, 14);
    RUN_TESTp(yacht_should_grow_and_add_and_remove_accurately_out_of_order, 8);
    RUN_TESTp(yacht_should_grow_and_add_and_remove_accurately_out_of_order, 10);
    RUN_TESTp(yacht_should_grow_and_add_and_remove_accurately_out_of_order, 14);

}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(suite);
    GREATEST_MAIN_END();        /* display results */
}
