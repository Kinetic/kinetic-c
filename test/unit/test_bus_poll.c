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
#include "atomic.h"

#include <errno.h>

#include "bus_poll.h"
#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_bus_ssl.h"
#include "mock_syscall.h"
#include "mock_bus_inward.h"
#include "mock_listener.h"
#include "util.h"

extern struct pollfd fds[1];
extern uint8_t read_buf[sizeof(uint8_t) + sizeof(uint16_t)];
extern int poll_errno;
extern int read_errno;

struct bus *b = NULL;
struct bus test_bus = {
    .log_level = 0,
};

void setUp(void) {
    b = &test_bus;
    memset(fds, 0, sizeof(fds));
    poll_errno = 0;
    read_errno = 0;
}

void tearDown(void) {
}

void test_BusPoll_OnCompletion_should_return_true_on_successful_case(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLIN;

    syscall_read_ExpectAndReturn(5, read_buf, sizeof(read_buf), sizeof(read_buf));
    read_buf[0] = LISTENER_MSG_TAG;
    read_buf[1] = 0x11;
    read_buf[2] = 0x22;
    Bus_BackpressureDelay_Expect(b, 0x2211, LISTENER_BACKPRESSURE_SHIFT);
    TEST_ASSERT_TRUE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}

void test_BusPoll_OnCompletion_should_retry_on_EINTR(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, -1);
    poll_errno = EINTR;
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLIN;

    syscall_read_ExpectAndReturn(5, read_buf, sizeof(read_buf), sizeof(read_buf));
    read_buf[0] = LISTENER_MSG_TAG;
    read_buf[1] = 0x11;
    read_buf[2] = 0x22;
    Bus_BackpressureDelay_Expect(b, 0x2211, LISTENER_BACKPRESSURE_SHIFT);
    TEST_ASSERT_TRUE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}

void test_BusPoll_OnCompletion_should_expose_poll_IO_error(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, -1);
    poll_errno = EINVAL;
    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 3));
}

void test_BusPoll_OnCompletion_should_expose_POLLHUP(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLHUP;

    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}

void test_BusPoll_OnCompletion_should_expose_POLLERR(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLERR;

    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}

void test_BusPoll_OnCompletion_should_expose_POLLNVAL(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLNVAL;

    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}

void test_BusPoll_OnCompletion_should_retry_on_read_EINTR(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLIN;

    syscall_read_ExpectAndReturn(5, read_buf, sizeof(read_buf), -1);
    read_errno = EINTR;

    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLIN;

    syscall_read_ExpectAndReturn(5, read_buf, sizeof(read_buf), sizeof(read_buf));
    read_buf[0] = LISTENER_MSG_TAG;
    read_buf[1] = 0x11;
    read_buf[2] = 0x22;
    Bus_BackpressureDelay_Expect(b, 0x2211, LISTENER_BACKPRESSURE_SHIFT);
    TEST_ASSERT_TRUE(BusPoll_OnCompletion(b, 5));
    TEST_ASSERT_EQUAL(5, fds[0].fd);
}


void test_BusPoll_OnCompletion_should_expose_read_IO_error(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLNVAL;

    read_errno = EIO;
    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 5));
}

void test_BusPoll_OnCompletion_should_handle_incorrect_read_size(void)
{
    syscall_poll_ExpectAndReturn(fds, 1, -1, 1);

    fds[0].revents |= POLLIN;

    syscall_read_ExpectAndReturn(5, read_buf, sizeof(read_buf), sizeof(read_buf) - 1);
    TEST_ASSERT_FALSE(BusPoll_OnCompletion(b, 5));
}
