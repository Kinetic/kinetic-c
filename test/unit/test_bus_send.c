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
#include "send.h"
#include "send_internal.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_poll.h"
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
extern int completion_pipe;

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

void test_Send_DoBlockingSend_should_reject_message_on_timestamp_failure(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, false);
    TEST_ASSERT_FALSE(Send_DoBlockingSend(b, box));
}

static void expect_notify_listener(bool ok) {
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    for (int i = 0; i < SEND_NOTIFY_LISTENER_RETRIES; i++) {
        completion_pipe = 41;
        Listener_HoldResponse_ExpectAndReturn(l, box->fd,
            box->out_seq_id, box->timeout_sec + 5, &completion_pipe, ok);
        if (ok) {
            BusPoll_OnCompletion_ExpectAndReturn(b, completion_pipe, true);
            return;
        }
        syscall_poll_ExpectAndReturn(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY, 0);
    }
}

void test_Send_DoBlockingSend_should_reject_message_if_listener_notify_fails(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(false);
    TEST_ASSERT_FALSE(Send_DoBlockingSend(b, box));
}

static void expect_handle_failure(void) {
    backpressure = 54321;
    Bus_ProcessBoxedMessage_ExpectAndReturn(b, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 54321, LISTENER_EXPECT_BACKPRESSURE_SHIFT);
}

void test_Send_DoBlockingSend_should_set_TX_FAILURE_if_second_timestamp_failure(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, false);
    expect_handle_failure();

    /* Note: This should return *true*, because the listener has already been
     * successfully notified to hold the response, so we need to use the
     * timeout and callback style of failure notification. */
    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_Send_DoBlockingSend_should_set_TX_FAILURE_on_poll_IO_error(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, -1);
    poll_errno = EIO;

    expect_handle_failure();
    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_Send_DoBlockingSend_should_set_UNREGISTERED_SOCKET_if_socket_is_unregistered(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLNVAL;

    expect_handle_failure();
    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_UNREGISTERED_SOCKET, box->result.status);
}

void test_Send_DoBlockingSend_should_set_TX_FAILURE_if_socket_is_closed(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLHUP;

    expect_handle_failure();
    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_TX_FAILURE, box->result.status);
}

void test_Send_DoBlockingSend_should_return_true_and_SUCCESS_status_if_write_completes(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLOUT;

    SendHelper_HandleWrite_ExpectAndReturn(b, box, SHHW_DONE);

    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
}

void test_Send_DoBlockingSend_should_return_true_and_status_set_by_callee_if_write_fails(void) {
    Util_Timestamp_ExpectAndReturn(&start, true, true);
    expect_notify_listener(true);
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(fds, 1, 11 * 1000, 1);
    fds[0].revents |= POLLOUT;

    box->result.status = BUS_SEND_RX_TIMEOUT;
    SendHelper_HandleWrite_ExpectAndReturn(b, box, SHHW_ERROR);

    TEST_ASSERT_TRUE(Send_DoBlockingSend(b, box));
    TEST_ASSERT_EQUAL(BUS_SEND_RX_TIMEOUT, box->result.status);
}
