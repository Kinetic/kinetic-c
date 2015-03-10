/*
* kinetic-c-client
* Copyright (C) 2015 Seagate Technology.
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
#include "send_helper.h"
#include "send_internal.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_listener.h"
#include "mock_send.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "listener_internal_types.h"

bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;

extern struct timeval done;
extern uint16_t backpressure;

static struct bus B = {
    .log_level = 0,
};
static struct listener Listener = {
    .bus = &B,
};

static uint8_t default_out_msg[] = "default_out_msg";

static boxed_msg Box = {
    .fd = 5,
    .out_seq_id = 12345,
    .timeout_sec = 11,
    .ssl = BUS_NO_SSL,
    .out_msg_size = sizeof(default_out_msg),
};

void setUp(void) {
    b = &B;
    box = &Box;
    l = &Listener;
    
    backpressure = 0;
    memset(&done, 0, sizeof(done));
}

void tearDown(void) {}

void test_send_helper_handle_write_should_notify_listener_and_succeed_when_writing_whole_message_over_plain_socket(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    listener_expect_response_ExpectAndReturn(l, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_send_helper_handle_write_should_fail_if_timestamp_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    util_timestamp_ExpectAndReturn(&done, true, false);
    send_handle_failure_Expect(b, box, BUS_SEND_TIMESTAMP_ERROR);
    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_send_helper_handle_write_should_fail_if_notifying_listener_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);

    for (int retries = 0; retries < SEND_NOTIFY_LISTENER_RETRIES; retries++) {
        listener_expect_response_ExpectAndReturn(l, box, &backpressure, false);
        syscall_poll_ExpectAndReturn(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY, 0);
    }
    send_handle_failure_Expect(b, box, BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_send_helper_handle_write_should_fail_if_socket_write_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, -1);
    errno = EPIPE;
    util_is_resumable_io_error_ExpectAndReturn(EPIPE, false);
    send_handle_failure_Expect(b, box, BUS_SEND_TX_FAILURE);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_send_helper_handle_write_should_retry_write_if_socket_write_gets_EINTR(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    errno = EINTR;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, -1);
    util_is_resumable_io_error_ExpectAndReturn(EINTR, true);
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    listener_expect_response_ExpectAndReturn(l, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_send_helper_handle_write_should_notify_listener_and_succeed_when_writing_sufficient_partial_writes_over_plain_socket(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    // Write the first part
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem - 5);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);
    TEST_ASSERT_EQUAL(rem - 5, box->out_sent_size);

    // Write the rest
    syscall_write_ExpectAndReturn(5, &box->out_msg[rem - 5], 5, 5);
    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    listener_expect_response_ExpectAndReturn(l, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_send_helper_handle_write_should_succeed_when_writing_whole_message_over_SSL_socket(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, rem);

    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    listener_expect_response_ExpectAndReturn(l, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_send_helper_handle_write_should_succeed_when_writing_whole_message_over_SSL_socket_in_multiple_pieces(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, rem - 5);
    TEST_ASSERT_EQUAL(0, box->out_sent_size);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[rem - 5], 5, 5);
    util_timestamp_ExpectAndReturn(&done, true, true);
    bus_get_listener_for_socket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    listener_expect_response_ExpectAndReturn(l, box, &backpressure, true);
    bus_backpressure_delay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);
    res = send_helper_handle_write(b, box);

    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_send_helper_handle_write_should_return_OK_and_go_back_to_poll_loop_when_SSL_write_returns_WANT_WRITE(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_WANT_WRITE);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);
}

void test_send_helper_handle_write_should_yield_TX_FAILURE_on_ERROR_SYSCALL_from_SSL(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_SYSCALL);
    errno = EINVAL;
    util_is_resumable_io_error_ExpectAndReturn(EINVAL, false);
    send_handle_failure_Expect(b, box, BUS_SEND_TX_FAILURE);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_send_helper_handle_write_should_yield_TX_FAILURE_on_ERROR_SSL_from_SSL(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_SSL);
    send_handle_failure_Expect(b, box, BUS_SEND_TX_FAILURE);

    send_helper_handle_write_res res = send_helper_handle_write(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}
