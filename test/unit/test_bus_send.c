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
#include "send.h"
#include "send_internal.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_listener.h"
#include "mock_send_helper.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "listener_internal_types.h"

extern struct timeval start;
extern struct timeval now;
extern struct pollfd fds[1];
extern size_t backpressure;
extern int poll_errno;
extern int write_errno;

struct bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;

struct listener *listeners[1];

static struct bus B = {
    .log_level = 0,
    .listeners = listeners,
    .listener_count = 1,
};
static struct listener Listener = {
    .bus = &B,
};
static boxed_msg Box = {
    .fd = 1,
    .out_seq_id = 12345,
    .timeout_sec = 11,
};

void setUp(void) {
    memset(fds, 0, sizeof(fds));
    b = &B;
    
    listeners[0] = &Listener;
    l = &Listener;
    
    box = &Box;
}

void tearDown(void) {}

void test_send_do_blocking_send_should_reject_message_on_timestamp_failure(void) {
    util_timestamp_ExpectAndReturn(&start, true, false);
    TEST_ASSERT_FALSE(send_do_blocking_send(b, box));
}

static void expect_notify_listener(bool ok) {
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    for (int i = 0; i < SEND_NOTIFY_LISTENER_RETRIES; i++) {
        listener_hold_response_ExpectAndReturn(l, box->fd,
            box->out_seq_id, box->timeout_sec + 5, ok);
        if (ok) { return; }
        syscall_poll_ExpectAndReturn(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY, 0);
    }
}

void test_send_do_blocking_send_should_reject_message_if_listener_notify_fails(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(false);
    TEST_ASSERT_FALSE(send_do_blocking_send(b, box));
}

static void expect_handle_failure(void) {
    backpressure = random();
    bus_process_boxed_message_ExpectAndReturn(b, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, backpressure, LISTENER_EXPECT_BACKPRESSURE_SHIFT);
}

void test_send_do_blocking_send_should_set_TX_FAILURE_if_second_timestamp_failure(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, false);
    expect_handle_failure();

    /* Note: This should return *true*, because the listener has already been
     * successfully notified to hold the response, so we need to use the
     * timeout and callback style of failure notification. */
    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_send_do_blocking_send_should_set_TX_FAILURE_on_poll_IO_error(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, -1);
    poll_errno = EIO;

    expect_handle_failure();
    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_send_do_blocking_send_should_set_UNREGISTERED_SOCKET_if_socket_is_unregistered(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLNVAL;

    expect_handle_failure();
    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_UNREGISTERED_SOCKET, box->result.status);
}

void test_send_do_blocking_send_should_set_TX_FAILURE_if_socket_is_closed(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLHUP;

    expect_handle_failure();
    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_send_do_blocking_send_should_return_true_and_SUCCESS_status_if_write_completes(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLOUT;

    send_helper_handle_write_ExpectAndReturn(b, box, SHHW_DONE);

    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_REQUEST_COMPLETE, box->result.status);
}

void test_send_do_blocking_send_should_return_true_and_status_set_by_callee_if_write_fails(void) {
    util_timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    util_timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLOUT;

    box->result.status = BUS_SEND_RX_TIMEOUT;
    send_helper_handle_write_ExpectAndReturn(b, box, SHHW_ERROR);

    TEST_ASSERT_TRUE(send_do_blocking_send(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_RX_TIMEOUT, box->result.status);
}
