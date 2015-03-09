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
#include "listener_cmd.h"
#include "listener_cmd_internal.h"
#include "listener_internal.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "mock_listener.h"
#include "mock_listener_helper.h"
#include "mock_listener_io.h"
#include "mock_listener_task.h"

struct bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;
extern uint8_t reply_buf[sizeof(uint8_t) + sizeof(uint16_t)];
extern uint8_t cmd_buf[LISTENER_CMD_BUF_SIZE];

static bus_sink_cb_res_t test_sink_cb(uint8_t *read_buf,
    size_t read_size, void *socket_udata);

static struct bus B = {
    .log_level = 0,
    .sink_cb = test_sink_cb,
};
static struct listener Listener;
static boxed_msg Box = {
    .fd = 1,
    .out_seq_id = 12345,
    .timeout_sec = 11,
};

static rx_info_t *NULL_INFO = ((rx_info_t *)-1);

void setUp(void) {
    b = &B;
    memset(&Listener, 0, sizeof(Listener));
    l = &Listener;
    l->bus = &B;
    box = &Box;
}

void tearDown(void) {}

void test_listener_ListenerCmd_NotifyCaller_should_write_tag_to_caller_fd(void) {
    int fd = 5;
    ListenerTask_GetBackpressure_ExpectAndReturn(l, 0);
    syscall_write_ExpectAndReturn(fd, reply_buf, sizeof(reply_buf), 3);
    ListenerCmd_NotifyCaller(l, fd);
    TEST_ASSERT_EQUAL(LISTENER_MSG_TAG, reply_buf[0]);
    TEST_ASSERT_EQUAL(0, reply_buf[1]);
    TEST_ASSERT_EQUAL(0, reply_buf[2]);
}

void test_listener_ListenerCmd_NotifyCaller_should_write_backpressure_to_reply_buffer(void) {
    int fd = 5;
    ListenerTask_GetBackpressure_ExpectAndReturn(l, 0x1234);
    syscall_write_ExpectAndReturn(fd, reply_buf, sizeof(reply_buf), 3);
    ListenerCmd_NotifyCaller(l, fd);
    TEST_ASSERT_EQUAL(0x34, reply_buf[1]);
    TEST_ASSERT_EQUAL(0x12, reply_buf[2]);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_return_on_error(void) {
    int res = 1;
    l->fds[INCOMING_MSG_PIPE].revents = POLLHUP;
    ListenerCmd_CheckIncomingMessages(l, &res);
}

bus_sink_cb_res_t test_sink_cb(uint8_t *read_buf,
    size_t read_size, void *socket_udata)
{
    (void)read_buf;
    (void)read_size;
    (void)socket_udata;
    return (bus_sink_cb_res_t) { .next_read = 31, };
}

static void expect_notify_caller(listener *l, int fd) {
    ListenerTask_GetBackpressure_ExpectAndReturn(l, 0);
    syscall_write_ExpectAndReturn(fd, reply_buf, sizeof(reply_buf), 3);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_redundant_ADD_SOCKET_command(void) {
    l->fds[INCOMING_MSG_PIPE_ID].fd = 5;
    l->fds[INCOMING_MSG_PIPE_ID].revents = POLLIN;
    l->read_buf = malloc(256);

    connection_info *ci = calloc(1, sizeof(*ci));
    *(int *)&ci->fd = 5;    

    listener_msg *msg = &l->msgs[0];
    msg->type = MSG_ADD_SOCKET;
    msg->u.add_socket.info = ci;
    msg->u.add_socket.notify_fd = 18;

    l->tracked_fds = 1;
    l->fds[INCOMING_MSG_PIPE].fd = ci->fd;

    cmd_buf[0] = 0;
    syscall_read_ExpectAndReturn(l->fds[INCOMING_MSG_PIPE_ID].fd, cmd_buf, sizeof(cmd_buf), 1);

    expect_notify_caller(l, 18);
    int res = 1;

    ListenerTask_ReleaseMsg_Expect(l, msg);
    ListenerCmd_CheckIncomingMessages(l, &res);
    
    TEST_ASSERT_EQUAL(0, res);
}

static void setup_command(listener_msg *pmsg, rx_info_t *info) {
    l->fds[INCOMING_MSG_PIPE_ID].fd = 5;
    l->fds[INCOMING_MSG_PIPE_ID].revents = POLLIN;
    l->read_buf = malloc(256);
    l->fds[INCOMING_MSG_PIPE].fd = 9;

    connection_info *ci = calloc(1, sizeof(*ci));
    *(int *)&ci->fd = 91;

    memcpy(&l->msgs[0], pmsg, sizeof(*pmsg));

    cmd_buf[0] = 0;
    syscall_read_ExpectAndReturn(l->fds[INCOMING_MSG_PIPE_ID].fd, cmd_buf, sizeof(cmd_buf), 1);
    if (pmsg->type == MSG_ADD_SOCKET) {
        ListenerTask_GrowReadBuf_ExpectAndReturn(l, 31, true);
    }
    
    if (info) {
        listener_helper_get_free_rx_info_ExpectAndReturn(l, info == NULL_INFO ? NULL : info);
    }
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_ADD_SOCKET_command(void) {
    connection_info *ci = calloc(1, sizeof(*ci));
    *(int *)&ci->fd = 91;

    listener_msg msg = {
        .type = MSG_ADD_SOCKET,
        .u.add_socket = {
            .info = ci,
            .notify_fd = 7,
        },
    };

    setup_command(&msg, NULL);
    int res = 1;

    l->tracked_fds = 3;
    expect_notify_caller(l, 7);
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);

    TEST_ASSERT_EQUAL(ci, l->fd_info[3]);
    TEST_ASSERT_EQUAL(ci->fd, l->fds[3 + INCOMING_MSG_PIPE].fd);
    TEST_ASSERT_EQUAL(31, ci->to_read_size);
    TEST_ASSERT_EQUAL(POLLIN, l->fds[3 + INCOMING_MSG_PIPE].events);
    TEST_ASSERT_EQUAL(4, l->tracked_fds);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_REMOVE_SOCKET_command(void) {
    listener_msg msg = {
        .id = 4,
        .type = MSG_REMOVE_SOCKET,
        .pipes = {7, 8},
        .u.remove_socket = {
            .fd = 50,
            .notify_fd = 100,
        },
    };
    setup_command(&msg, NULL);
    expect_notify_caller(l, 100);

    connection_info *ci = calloc(1, sizeof(*ci));
    l->tracked_fds = 2;
    l->fds[1 + INCOMING_MSG_PIPE].fd = 50;
    l->fd_info[1] = ci;
    
    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(1, l->tracked_fds);
    TEST_ASSERT_EQUAL(0, res);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_NULL_info_failure_case(void) {
    listener_msg msg = {
        .id = 1,
        .type = MSG_HOLD_RESPONSE,
        .pipes = {9, 10},
        .u.hold = {
            .fd = 23,
            .seq_id = 12345,
            .timeout_sec = 9,
        },
    };

    setup_command(&msg, NULL_INFO);

    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(0, res);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_HOLD_RESPONSE_command(void) {
    listener_msg msg = {
        .id = 1,
        .type = MSG_HOLD_RESPONSE,
        .pipes = {9, 10},
        .u.hold = {
            .fd = 23,
            .seq_id = 12345,
            .timeout_sec = 9,
        },
    };

    rx_info_t info = {
        .state = RIS_INACTIVE,
    };
    setup_command(&msg, &info);

    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(0, res);

    TEST_ASSERT_EQUAL(RIS_HOLD, info.state);
    TEST_ASSERT_EQUAL(9, info.timeout_sec);
    TEST_ASSERT_EQUAL(23, info.u.hold.fd);
    TEST_ASSERT_EQUAL(false, info.u.hold.has_result);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_EXPECT_command_when_result_is_saved(void) {
    listener_msg msg = {
        .id = 1,
        .type = MSG_EXPECT_RESPONSE,
        .pipes = {9, 10},
        .u.expect.box = box,
    };

    void *opaque_result = (void *)((uintptr_t)-25);

    bus_unpack_cb_res_t hold_result = {
        .ok = true,
        .u.success = {
            .msg = opaque_result,
            .seq_id = 12345,
        },
    };

    rx_info_t hold_info = {
        .state = RIS_HOLD,
        .timeout_sec = 9,
        .u.hold = {
            .fd = 23,
            .has_result = true,
            .result = hold_result,
        },
    };

    setup_command(&msg, NULL);
    listener_helper_find_info_by_sequence_id_ExpectAndReturn(l, box->fd, box->out_seq_id, &hold_info);
    ListenerTask_AttemptDelivery_Expect(l, &hold_info);
    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(0, res);

    TEST_ASSERT_EQUAL(RIS_EXPECT, hold_info.state);
    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, hold_info.u.expect.error);
    TEST_ASSERT_EQUAL(box, hold_info.u.expect.box);
    TEST_ASSERT_EQUAL(true, hold_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(opaque_result, hold_info.u.expect.result.u.success.msg);
    TEST_ASSERT_EQUAL(12345, hold_info.u.expect.result.u.success.seq_id);
}

void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_EXPECT_command_when_no_result_is_saved(void) {
    listener_msg msg = {
        .id = 1,
        .type = MSG_EXPECT_RESPONSE,
        .pipes = {9, 10},
        .u.expect.box = box,
    };

    void *opaque_result = (void *)((uintptr_t)-25);

    rx_info_t hold_info = {
        .state = RIS_HOLD,
        .timeout_sec = 9,
        .u.hold = {
            .fd = 23,
            .has_result = false,
        },
    };

    setup_command(&msg, NULL);
    listener_helper_find_info_by_sequence_id_ExpectAndReturn(l, box->fd, box->out_seq_id, &hold_info);
    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(0, res);

    TEST_ASSERT_EQUAL(RIS_EXPECT, hold_info.state);
    TEST_ASSERT_EQUAL(box, hold_info.u.expect.box);
    TEST_ASSERT_EQUAL(RX_ERROR_NONE, hold_info.u.expect.error);
    TEST_ASSERT_EQUAL(false, hold_info.u.expect.has_result);
    TEST_ASSERT_EQUAL(11, hold_info.timeout_sec);
}
    
void test_listener_ListenerCmd_CheckIncomingMessages_should_handle_incoming_SHUTDOWN_command(void) {
    listener_msg msg = {
        .id = 1,
        .type = MSG_SHUTDOWN,
        .pipes = {9, 10},
        .u.shutdown.notify_fd = 123,
    };

    l->shutdown_notify_fd = LISTENER_NO_FD;
    setup_command(&msg, NULL);

    int res = 1;
    ListenerTask_ReleaseMsg_Expect(l, &l->msgs[0]);
    ListenerCmd_CheckIncomingMessages(l, &res);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(123, l->shutdown_notify_fd);
}
