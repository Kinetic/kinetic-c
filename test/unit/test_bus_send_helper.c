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

void test_SendHelper_HandleWrite_should_notify_listener_and_succeed_when_writing_whole_message_over_plain_socket(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
    TEST_ASSERT_EQUAL(BUS_SEND_REQUEST_COMPLETE, box->result.status);
}

void test_SendHelper_HandleWrite_should_fail_if_timestamp_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    Util_Timestamp_ExpectAndReturn(&done, true, false);
    Send_HandleFailure_Expect(b, box, BUS_SEND_TIMESTAMP_ERROR);
    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_SendHelper_HandleWrite_should_fail_if_notifying_listener_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);

    for (int retries = 0; retries < SEND_NOTIFY_LISTENER_RETRIES; retries++) {
        Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, false);
        syscall_poll_ExpectAndReturn(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY, 0);
    }
    Send_HandleFailure_Expect(b, box, BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_SendHelper_HandleWrite_should_fail_if_socket_write_fails(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, -1);
    errno = EPIPE;
    Util_IsResumableIOError_ExpectAndReturn(EPIPE, false);
    Send_HandleFailure_Expect(b, box, BUS_SEND_TX_FAILURE);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_SendHelper_HandleWrite_should_retry_write_if_socket_write_gets_EINTR(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;
    errno = EINTR;
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, -1);
    Util_IsResumableIOError_ExpectAndReturn(EINTR, true);
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem);

    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_SendHelper_HandleWrite_should_notify_listener_and_succeed_when_writing_sufficient_partial_writes_over_plain_socket(void) {
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    // Write the first part
    syscall_write_ExpectAndReturn(5, &box->out_msg[0], rem, rem - 5);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);
    TEST_ASSERT_EQUAL(rem - 5, box->out_sent_size);

    // Write the rest
    syscall_write_ExpectAndReturn(5, &box->out_msg[rem - 5], 5, 5);
    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_SendHelper_HandleWrite_should_succeed_when_writing_whole_message_over_SSL_socket(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, rem);

    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_SendHelper_HandleWrite_should_succeed_when_writing_whole_message_over_SSL_socket_in_multiple_pieces(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, rem - 5);
    TEST_ASSERT_EQUAL(0, box->out_sent_size);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[rem - 5], 5, 5);
    Util_Timestamp_ExpectAndReturn(&done, true, true);
    Bus_GetListenerForSocket_ExpectAndReturn(b, box->fd, l);
    backpressure = 0x1234;
    Listener_ExpectResponse_ExpectAndReturn(l, box, &backpressure, true);
    Bus_BackpressureDelay_Expect(b, 0x1234, LISTENER_EXPECT_BACKPRESSURE_SHIFT);
    res = SendHelper_HandleWrite(b, box);

    TEST_ASSERT_EQUAL(SHHW_DONE, res);
}

void test_SendHelper_HandleWrite_should_return_OK_and_go_back_to_poll_loop_when_SSL_write_returns_WANT_WRITE(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_WANT_WRITE);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_OK, res);
}

void test_SendHelper_HandleWrite_should_yield_TX_FAILURE_on_ERROR_SYSCALL_from_SSL(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_SYSCALL);
    errno = EINVAL;
    Util_IsResumableIOError_ExpectAndReturn(EINVAL, false);
    Send_HandleFailure_Expect(b, box, BUS_SEND_TX_FAILURE);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}

void test_SendHelper_HandleWrite_should_yield_TX_FAILURE_on_ERROR_SSL_from_SSL(void) {
    SSL fake_ssl;
    box->ssl = &fake_ssl;
    box->out_sent_size = 0;
    size_t rem = box->out_msg_size;

    syscall_SSL_write_ExpectAndReturn(&fake_ssl, &box->out_msg[0], rem, -1);
    syscall_SSL_get_error_ExpectAndReturn(&fake_ssl, -1, SSL_ERROR_SSL);
    Send_HandleFailure_Expect(b, box, BUS_SEND_TX_FAILURE);

    SendHelper_HandleWrite_res res = SendHelper_HandleWrite(b, box);
    TEST_ASSERT_EQUAL(SHHW_ERROR, res);
}
