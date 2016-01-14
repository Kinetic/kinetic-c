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
#include "listener_io.h"
#include "listener_internal.h"
#include "listener_internal_types.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "mock_listener.h"
#include "mock_listener_helper.h"
#include "mock_listener_cmd.h"
#include "mock_listener_task.h"

struct bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;

static bus_sink_cb_res_t sink_cb(uint8_t *read_buf,
    size_t read_size, void *socket_udata);
static bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata);
static void unexpected_msg_cb(void *msg,
    int64_t seq_id, void *bus_udata, void *socket_udata);
static void error_cb(bus_unpack_cb_res_t result, void *socket_udata);

static struct bus B = {
    .log_level = 0,
    .sink_cb = sink_cb,
    .unpack_cb = unpack_cb,
    .unexpected_msg_cb = unexpected_msg_cb,
    .error_cb = error_cb,
};
static struct listener Listener = {
    .bus = &B,
};
static boxed_msg Box = {
    .fd = 1,
    .out_seq_id = 12345,
    .timeout_sec = 11,
    .result.status = BUS_SEND_REQUEST_COMPLETE,
};

struct test_progress_info {
    size_t to_read;
    size_t read;
};

void setUp(void) {
    b = &B;
    memset(&Listener, 0, sizeof(Listener));
    l = &Listener;
    l->bus = &B;
    l->tracked_fds = 0;
    l->inactive_fds = 0;
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        info->state = RIS_INACTIVE;
    }
    Box.out_seq_id = 12345;

    box = &Box;
}

void tearDown(void) {}

void test_ListenerIO_AttemptRecv_should_handle_hangups_single_fd(void) {
    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_HOLD;
    info1->u.hold.fd = 5;  // non-matching FD, should be skipped

    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;  // match
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLHUP;  // hangup

    connection_info ci0 = {
        .fd = 5,
    };
    l->fd_info[0] = &ci0;

    l->tracked_fds = 1;
    l->inactive_fds = 0;
    l->rx_info_max_used = 1;

    ListenerIO_AttemptRecv(l, 1);

    // socket with error (5) should get moved to end
    TEST_ASSERT_EQUAL(5, l->fds[0 + INCOMING_MSG_PIPE].fd);
    TEST_ASSERT_EQUAL(1, l->inactive_fds);

    // should no longer attempt to read socket; timeout will close it
    TEST_ASSERT_EQUAL(0, l->fds[0 + INCOMING_MSG_PIPE].events);

    /* HOLD message should be marked with error */
    TEST_ASSERT_EQUAL(RX_ERROR_POLLHUP, info1->u.hold.error);
}

void test_ListenerIO_AttemptRecv_should_handle_hangups(void) {
    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_HOLD;
    info1->u.hold.fd = 100;  // non-matching FD, should be skipped

    rx_info_t *info2 = &l->rx_info[2];
    info2->state = RIS_HOLD;
    info2->u.hold.fd = 5; // matching FD

    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;  // match
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLHUP;  // hangup

    l->fds[1 + INCOMING_MSG_PIPE].fd = 100;
    l->fds[1 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = 0;  // no event

    connection_info ci0 = {
        .fd = 5,
    };
    l->fd_info[0] = &ci0;

    connection_info ci1 = {
        .fd = 100,
    };
    l->fd_info[1] = &ci1;

    l->tracked_fds = 2;
    l->inactive_fds = 0;
    l->rx_info_max_used = 2;

    ListenerIO_AttemptRecv(l, 1);

    // socket with error (5) should get moved to end
    TEST_ASSERT_EQUAL(100, l->fds[0 + INCOMING_MSG_PIPE].fd);
    TEST_ASSERT_EQUAL(5, l->fds[1 + INCOMING_MSG_PIPE].fd);

    TEST_ASSERT_EQUAL(1, l->inactive_fds);

    // should no longer attempt to read socket; timeout will close it
    TEST_ASSERT_EQUAL(0, l->fds[1 + INCOMING_MSG_PIPE].events);

    /* HOLD message should be marked with error */
    TEST_ASSERT_EQUAL(RX_ERROR_POLLHUP, info2->u.hold.error);
}

void test_ListenerIO_AttemptRecv_should_handle_socket_errors(void) {
    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_HOLD;
    info1->u.hold.fd = 100;  // non-matching FD, should be skipped

    rx_info_t *info2 = &l->rx_info[2];
    info2->state = RIS_HOLD;
    info2->u.hold.fd = 5; // matching FD

    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;  // match
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLERR;  // socket error

    l->fds[1 + INCOMING_MSG_PIPE].fd = 100;
    l->fds[1 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = 0;  // no event

    connection_info ci0 = {
        .fd = 5,
    };
    l->fd_info[0] = &ci0;

    connection_info ci1 = {
        .fd = 100,
    };
    l->fd_info[1] = &ci1;

    l->tracked_fds = 2;
    l->inactive_fds = 0;
    l->rx_info_max_used = 2;

    ListenerIO_AttemptRecv(l, 1);

    // socket with error (5) should get moved to end
    TEST_ASSERT_EQUAL(100, l->fds[0 + INCOMING_MSG_PIPE].fd);
    TEST_ASSERT_EQUAL(5, l->fds[1 + INCOMING_MSG_PIPE].fd);

    TEST_ASSERT_EQUAL(1, l->inactive_fds);

    // should no longer attempt to read socket; timeout will close it
    TEST_ASSERT_EQUAL(0, l->fds[1 + INCOMING_MSG_PIPE].events);

    // HOLD message should be marked with error
    TEST_ASSERT_EQUAL(RX_ERROR_POLLERR, info1->u.hold.error);
}

static uint8_t the_result[1];

static bus_sink_cb_res_t sink_cb(uint8_t *read_buf,
        size_t read_size, void *socket_udata) {

    void *result = NULL;

    assert(socket_udata);
    struct test_progress_info *pi = (struct test_progress_info *)socket_udata;
    pi->read += read_size;
    if (pi->read == pi->to_read) {
        result = the_result;
    }
    size_t next_read = pi->to_read - pi->read;
    bus_sink_cb_res_t res = {
        .next_read = next_read,
        .full_msg_buffer = result,
    };
    (void)read_buf;
    (void)read_size;
    (void)socket_udata;
    return res;
}

static bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata) {
    assert(msg == the_result);
    bus_unpack_cb_res_t res = {
        .ok = true,
        .u.success = {
            .msg = the_result,
            .seq_id = 12345,
        },
    };
    (void)msg;
    (void)socket_udata;
    return res;
}

static void unexpected_msg_cb(void *msg,
        int64_t seq_id, void *bus_udata, void *socket_udata) {
    (void)msg;
    (void)seq_id;
    (void)bus_udata;
    (void)socket_udata;
}

static void error_cb(bus_unpack_cb_res_t result, void *socket_udata) {
    (void)result;
    (void)socket_udata;
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_PLAIN,
        .to_read_size = 123,
        .udata = &progress_info,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, ci.to_read_size);

    rx_info_t unpack_res_info = {
        .state = RIS_EXPECT,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);
    ListenerTask_AttemptDelivery_Expect(l, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, unpack_res_info.u.expect.error);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.expect.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.expect.result.u.success.msg);
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message_in_hold_state(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_PLAIN,
        .to_read_size = 123,
        .udata = &progress_info,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_HOLD;
    box->fd = 5;

    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, ci.to_read_size);

    rx_info_t unpack_res_info = {
        .state = RIS_HOLD,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(true, unpack_res_info.u.hold.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.hold.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.hold.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.hold.result.u.success.msg);
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message_in_multiple_pieces(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_PLAIN,
        .to_read_size = 123,
        .udata = &progress_info,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, ci.to_read_size - 1);
    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, 1, 1);

    rx_info_t unpack_res_info = {
        .state = RIS_EXPECT,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);
    ListenerTask_AttemptDelivery_Expect(l, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, unpack_res_info.u.expect.error);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.expect.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.expect.result.u.success.msg);
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message_on_EINTR_followed_by_successful_read(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_PLAIN,
        .to_read_size = 123,
        .udata = &progress_info,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    errno = EINTR;
    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, -1);
    Util_IsResumableIOError_ExpectAndReturn(EINTR, true);

    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, ci.to_read_size);

    rx_info_t unpack_res_info = {
        .state = RIS_EXPECT,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);
    ListenerTask_AttemptDelivery_Expect(l, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, unpack_res_info.u.expect.error);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.expect.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.expect.result.u.success.msg);
}

void test_ListenerIO_AttemptRecv_should_handle_socket_hangup_during_read(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_PLAIN,
        .to_read_size = 123,
        .udata = &progress_info,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->inactive_fds = 0;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    syscall_read_ExpectAndReturn(ci.fd, l->read_buf, ci.to_read_size, -1);
    errno = ECONNRESET;
    Util_IsResumableIOError_ExpectAndReturn(errno, false);
    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(0, l->fds[0 + INCOMING_MSG_PIPE].events & POLLIN);
    TEST_ASSERT_EQUAL(1, l->inactive_fds);
    TEST_ASSERT_EQUAL(RX_ERROR_READ_FAILURE, ci.error);
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message_over_SSL(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    SSL fake_ssl;
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_SSL,
        .to_read_size = 123,
        .udata = &progress_info,
        .ssl = &fake_ssl,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    syscall_SSL_read_ExpectAndReturn(ci.ssl, l->read_buf, ci.to_read_size, ci.to_read_size);

    rx_info_t unpack_res_info = {
        .state = RIS_EXPECT,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);
    ListenerTask_AttemptDelivery_Expect(l, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, unpack_res_info.u.expect.error);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.expect.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.expect.result.u.success.msg);
}

void test_ListenerIO_AttemptRecv_should_handle_successful_socket_read_and_unpack_message_in_multiple_pieces_over_SSL(void) {
    l->fds[0 + INCOMING_MSG_PIPE].fd = 5;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[0 + INCOMING_MSG_PIPE].revents = POLLIN;
    struct test_progress_info progress_info = {
        .to_read = 123,
    };
    SSL fake_ssl;
    connection_info ci = {
        .fd = 5,
        .type = BUS_SOCKET_SSL,
        .to_read_size = 123,
        .udata = &progress_info,
        .ssl = &fake_ssl,
    };
    l->fd_info[0] = &ci;
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;

    l->read_buf = calloc(256, sizeof(uint8_t));
    l->read_buf_size = 256;

    rx_info_t *info = &l->rx_info[0];
    info->state = RIS_EXPECT;
    box->fd = 5;
    info->u.expect.box = box;

    syscall_SSL_read_ExpectAndReturn(ci.ssl, l->read_buf, ci.to_read_size, ci.to_read_size - 1);

    syscall_SSL_read_ExpectAndReturn(ci.ssl, l->read_buf, 1, 1);

    rx_info_t unpack_res_info = {
        .state = RIS_EXPECT,
    };
    ListenerHelper_FindInfoBySequenceID_ExpectAndReturn(l, ci.fd, 12345, &unpack_res_info);
    ListenerTask_AttemptDelivery_Expect(l, &unpack_res_info);

    ListenerIO_AttemptRecv(l, 1);

    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, unpack_res_info.u.expect.error);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(true, unpack_res_info.u.expect.result.ok);
    TEST_ASSERT_EQUAL(12345, unpack_res_info.u.expect.result.u.success.seq_id);
    TEST_ASSERT_EQUAL(the_result, unpack_res_info.u.expect.result.u.success.msg);
}
