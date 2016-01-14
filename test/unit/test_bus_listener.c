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
#include "listener.h"
#include "listener_internal.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "mock_listener_helper.h"
#include "mock_listener_cmd.h"
#include "mock_listener_io.h"
#include "mock_listener_task.h"

struct bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;

static struct bus B = {
    .log_level = 0,
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

void setUp(void) {
    b = &B;
    l = &Listener;
    box = &Box;
}

void tearDown(void) {}

void test_Listener_AddSocket_should_handle_msg_exhaustion(void) {
    l->msg_freelist = NULL;
    connection_info ci;
    int fd = -1;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, NULL);
    TEST_ASSERT_FALSE(Listener_AddSocket(l, &ci, &fd));
}

void test_Listener_AddSocket_should_handle_full_msg_queue(void) {
    l->msg_freelist = NULL;
    connection_info ci;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, false);
    TEST_ASSERT_FALSE(Listener_AddSocket(l, &ci, &fd));
}

void test_Listener_AddSocket_should_add_ADD_SOCKET_msg_to_queue(void) {
    l->msg_freelist = NULL;
    connection_info ci;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, true);
    TEST_ASSERT_TRUE(Listener_AddSocket(l, &ci, &fd));

    TEST_ASSERT_EQUAL(MSG_ADD_SOCKET, msg.type);
    TEST_ASSERT_EQUAL(&ci, msg.u.add_socket.info);
    TEST_ASSERT_EQUAL(msg.pipes[1], msg.u.add_socket.notify_fd);
}

void test_Listener_RemoveSocket_should_handle_msg_exhaustion(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, NULL);
    TEST_ASSERT_FALSE(Listener_RemoveSocket(l, fd, &fd));
}

void test_Listener_RemoveSocket_should_handle_full_msg_queue(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, false);
    TEST_ASSERT_FALSE(Listener_RemoveSocket(l, fd, &fd));
}

void test_Listener_RemoveSocket_should_add_ADD_SOCKET_msg_to_queue(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, true);
    TEST_ASSERT_TRUE(Listener_RemoveSocket(l, fd, &fd));

    TEST_ASSERT_EQUAL(MSG_REMOVE_SOCKET, msg.type);
    TEST_ASSERT_EQUAL(fd, msg.u.remove_socket.fd);
    TEST_ASSERT_EQUAL(msg.pipes[1], msg.u.remove_socket.notify_fd);
}

void test_Listener_Shutdown_should_handle_msg_exhaustion(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, NULL);
    TEST_ASSERT_FALSE(Listener_Shutdown(l, &fd));
}

void test_Listener_Shutdown_should_handle_full_msg_queue(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, false);
    TEST_ASSERT_FALSE(Listener_Shutdown(l, &fd));
}

void test_Listener_Shutdown_should_add_ADD_SOCKET_msg_to_queue(void) {
    l->msg_freelist = NULL;
    int fd = -1;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &fd, true);
    TEST_ASSERT_TRUE(Listener_Shutdown(l, &fd));

    TEST_ASSERT_EQUAL(MSG_SHUTDOWN, msg.type);
}

void test_Listener_HoldResponse_should_enqueue_HOLD_RESPONSE_msg(void) {
    int socket = 7;
    int64_t seq_id = 12345;
    int16_t timeout_sec = 9;
    listener_msg msg;
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    int notify_fd = -1;
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, &notify_fd, true);
    msg.pipes[1] = 123;
    TEST_ASSERT_TRUE(Listener_HoldResponse(l, socket, seq_id, timeout_sec, &notify_fd));
    TEST_ASSERT_EQUAL(MSG_HOLD_RESPONSE, msg.type);
    TEST_ASSERT_EQUAL(socket, msg.u.hold.fd);
    TEST_ASSERT_EQUAL(seq_id, msg.u.hold.seq_id);
    TEST_ASSERT_EQUAL(123, msg.u.hold.notify_fd);
    TEST_ASSERT_EQUAL(timeout_sec, msg.u.hold.timeout_sec);
}

void test_Listener_ExpectResponse_should_enqueue_EXPECT_RESPONSE_msg(void) {
    listener_msg msg;
    struct boxed_msg box = {
        .fd = 0,
        .result.status = BUS_SEND_REQUEST_COMPLETE,
    };
    ListenerHelper_GetFreeMsg_ExpectAndReturn(l, &msg);
    uint16_t backpressure = 0;
    ListenerTask_GetBackpressure_ExpectAndReturn(l, 0x4321);
    ListenerHelper_PushMessage_ExpectAndReturn(l, &msg, NULL, true);

    TEST_ASSERT_TRUE(Listener_ExpectResponse(l, &box, &backpressure));
    TEST_ASSERT_EQUAL(MSG_EXPECT_RESPONSE, msg.type);
    TEST_ASSERT_EQUAL(&box, msg.u.expect.box);
    TEST_ASSERT_EQUAL(0x4321, backpressure);
}

void test_Listener_Free_on_NULL_should_be_a_no_op(void) {
    Listener_Free(NULL);
}

void test_Listener_Free_should_unblock_pending_callers_and_close_file_handles(void) {
    /* setup */
    struct listener *nl = calloc(1, sizeof(*nl));
    nl->commit_pipe = 37;
    nl->incoming_msg_pipe = 149;
    nl->shutdown_notify_fd = LISTENER_SHUTDOWN_COMPLETE_FD;
    nl->read_buf = calloc(32, sizeof(uint32_t));

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        nl->rx_info[i].state = RIS_INACTIVE;
    }
    for (int i = 0; i < MAX_QUEUE_MESSAGES; i++) {
        nl->msgs[i].pipes[0] = i;
        nl->msgs[i].pipes[1] = 2*i;
    }
    nl->msgs[4].type = MSG_ADD_SOCKET;
    nl->msgs[4].u.add_socket.notify_fd = 1234;

    nl->msgs[7].type = MSG_REMOVE_SOCKET;
    nl->msgs[7].u.remove_socket.notify_fd = 1237;

    /* test cleanup */
    for (int i = 0; i < MAX_QUEUE_MESSAGES; i++) {
        if (i == 4) {
            ListenerCmd_NotifyCaller_Expect(nl, 1234);
        } else if (i == 7) {
            ListenerCmd_NotifyCaller_Expect(nl, 1237);
        }

        syscall_close_ExpectAndReturn(i, 0);
        syscall_close_ExpectAndReturn(2*i, 0);
    }
    syscall_close_ExpectAndReturn(37, 0);
    syscall_close_ExpectAndReturn(149, 0);

    Listener_Free(nl);
}

