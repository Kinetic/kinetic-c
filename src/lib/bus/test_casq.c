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

#include <stdint.h>

#include "greatest.h"
#include "casq.h"

typedef struct casq casq;

TEST casq_should_cleanly_init_and_free(void) {
    casq *q = casq_new();
    casq_free(q, NULL, NULL);
    PASS();
}

TEST casq_should_push_and_pop_in_fifo_order(void) {
    casq *q = casq_new();

    for (uintptr_t i = 1; i < 10; i++) {
        casq_push(q, (void *)i);
    }

    for (uintptr_t i = 1; i < 10; i++) {
        uintptr_t v = (uintptr_t)casq_pop(q);
        ASSERT_EQ(i, v);
    }        
    
    casq_free(q, NULL, NULL);
    PASS();
}

static void free_cb(void *data, void *udata) {
    uintptr_t key = (uintptr_t)data;
    bool *free_table = (bool *)udata;

    free_table[key] = true;
}

TEST casq_free_should_destruct_data_in_queue(void) {
    casq *q = casq_new();

    bool free_table[10] = {0};

    for (uintptr_t i = 1; i < 10; i++) {
        casq_push(q, (void *)i);
    }

    casq_free(q, free_cb, (void *)free_table);

    for (uintptr_t i = 1; i < 10; i++) {
        ASSERT(free_table[i]);
    }
    PASS();
}

TEST casq_empty_should_return_whether_data_is_available(void) {
    casq *q = casq_new();

    for (uintptr_t i = 1; i < 10; i++) {
        ASSERT_EQ(i == 1, casq_empty(q));
        casq_push(q, (void *)i);
        ASSERT_EQ(false, casq_empty(q));
    }

    for (uintptr_t i = 1; i < 10; i++) {
        ASSERT_EQ(false, casq_empty(q));
        uintptr_t v = (uintptr_t)casq_pop(q);
        ASSERT_EQ(i == 9, casq_empty(q));
    }
    
    ASSERT_EQ(true, casq_empty(q));
    casq_free(q, NULL, NULL);
    PASS();
}

TEST casq_regression(void) {
    casq *q = casq_new();

    for (uintptr_t i = 1; i <= 10; i++) {
        ASSERT(casq_push(q, (void *)i));

        uintptr_t v = (uintptr_t)casq_pop(q);
        ASSERT_EQ(i, v);
    }

    ASSERT(casq_push(q, (void *)11));
    ASSERT(casq_push(q, (void *)12));
    uintptr_t v11 = (uintptr_t)casq_pop(q);
    ASSERT_EQ(11, v11);
    uintptr_t v12 = (uintptr_t)casq_pop(q);
    ASSERT_EQ(12, v12);
    uintptr_t v13 = (uintptr_t)casq_pop(q);
    ASSERT_EQ(0, v13);

    casq_free(q, NULL, NULL);
    PASS();
}

SUITE(suite) {
    RUN_TEST(casq_should_cleanly_init_and_free);
    RUN_TEST(casq_should_push_and_pop_in_fifo_order);
    RUN_TEST(casq_free_should_destruct_data_in_queue);
    RUN_TEST(casq_empty_should_return_whether_data_is_available);
    RUN_TEST(casq_regression);
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(suite);
    GREATEST_MAIN_END();        /* display results */
}
