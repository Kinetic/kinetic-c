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
#include "listener_helper.h"
#include "listener_internal.h"
#include "listener_internal_types.h"
#include "atomic.h"

#include <errno.h>

#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "mock_syscall.h"
#include "mock_util.h"
#include "mock_listener.h"
#include "mock_listener_io.h"
#include "mock_listener_cmd.h"
#include "mock_listener_task.h"

struct bus *b = NULL;
boxed_msg *box = NULL;
struct listener *l = NULL;
void *last_msg = NULL;
int64_t last_seq_id = BUS_NO_SEQ_ID;
void *last_bus_udata = NULL;
void *last_socket_udata = NULL;

extern struct timeval now;
extern struct timeval cur;
extern size_t backpressure;
extern int poll_res;
extern uint8_t msg_buf[sizeof(uint8_t)];

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
};

void setUp(void)
{
    b = &B;
    l = &Listener;
    box = &Box;
    l->msgs_in_use = 0;
    l->rx_info_in_use = 0;
    l->upstream_backpressure = 0;
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        l->rx_info[i].state = RIS_INACTIVE;
        *(int *)&l->rx_info[i].id = i;
    }

    last_msg = NULL;
    last_seq_id = BUS_NO_SEQ_ID;
    last_bus_udata = NULL;
    last_socket_udata = NULL;
}

void tearDown(void) {}

void test_ListenerHelper_GetFreeMsg_should_reserve_a_message_if_available(void)
{
    listener_msg *msg = &l->msgs[9];
    l->msg_freelist = msg;
    l->msgs_in_use = 7;
    TEST_ASSERT_EQUAL(msg, ListenerHelper_GetFreeMsg(l));
    TEST_ASSERT_EQUAL(8, l->msgs_in_use);
}

void test_ListenerHelper_GetFreeMsg_should_expose_running_out_of_messages(void)
{
    l->msg_freelist = NULL;
    l->msgs_in_use = 7;
    TEST_ASSERT_EQUAL(NULL, ListenerHelper_GetFreeMsg(l));
    TEST_ASSERT_EQUAL(7, l->msgs_in_use);
}

void test_ListenerHelper_PushMessage_should_add_a_message_to_the_listeners_command_queue(void)
{
    l->commit_pipe = 100;
    listener_msg *msg = &l->msgs[9];
    memset(msg_buf, 0xFF, sizeof(msg_buf));
    msg->pipes[0] = 23;

    syscall_write_ExpectAndReturn(l->commit_pipe, msg_buf, sizeof(msg_buf), sizeof(msg_buf));

    int reply_fd = -1;
    TEST_ASSERT_TRUE(ListenerHelper_PushMessage(l, msg, &reply_fd));
    TEST_ASSERT_EQUAL(23, reply_fd);
}

void test_ListenerHelper_PushMessage_should_retry_and_add_a_message_to_the_listeners_command_queue_on_EINTR(void)
{
    l->commit_pipe = 100;
    listener_msg *msg = &l->msgs[9];
    memset(msg_buf, 0xFF, sizeof(msg_buf));
    msg->pipes[0] = 23;

    errno = EINTR;
    syscall_write_ExpectAndReturn(l->commit_pipe, msg_buf, sizeof(msg_buf), -1);
    syscall_write_ExpectAndReturn(l->commit_pipe, msg_buf, sizeof(msg_buf), sizeof(msg_buf));

    int reply_fd = -1;
    TEST_ASSERT_TRUE(ListenerHelper_PushMessage(l, msg, &reply_fd));
    TEST_ASSERT_EQUAL(23, reply_fd);
}

void test_ListenerHelper_PushMessage_should_expose_errors(void)
{
    l->commit_pipe = 100;
    listener_msg *msg = &l->msgs[9];
    memset(msg_buf, 0xFF, sizeof(msg_buf));
    msg->pipes[0] = 23;

    errno = EIO;
    syscall_write_ExpectAndReturn(l->commit_pipe, msg_buf, sizeof(msg_buf), -1);

    int reply_fd = -1;
    ListenerTask_ReleaseMsg_Expect(l, msg);
    TEST_ASSERT_FALSE(ListenerHelper_PushMessage(l, msg, &reply_fd));
}

void test_ListenerHelper_GetFreeRXInfo_should_return_a_free_RX_INFO(void)
{
    struct rx_info_t *head = &l->rx_info[123];
    head->next = &l->rx_info[55];
    l->rx_info_in_use = 8;
    l->rx_info_freelist = head;
    l->rx_info_max_used = 122;
    TEST_ASSERT_EQUAL(head, ListenerHelper_GetFreeRXInfo(l));
    TEST_ASSERT_EQUAL(&l->rx_info[55], l->rx_info_freelist);
    TEST_ASSERT_EQUAL(9, l->rx_info_in_use);
    TEST_ASSERT_EQUAL(head->id, l->rx_info_max_used);
}

void test_ListenerHelper_GetFreeRXInfo_should_expose_errors(void)
{
    l->rx_info_freelist = NULL;
    TEST_ASSERT_EQUAL(NULL, ListenerHelper_GetFreeRXInfo(l));
}

void test_ListenerHelper_FindInfoBySequenceID_should_find_info_by_sequence_id(void)
{
    l->rx_info_max_used = 100;
    struct rx_info_t *info = &l->rx_info[95];
    info->state = RIS_HOLD;
    info->u.hold.fd = 75;
    info->u.hold.seq_id = 12345;
    TEST_ASSERT_EQUAL(info, ListenerHelper_FindInfoBySequenceID(l, 75, 12345));
}

void test_ListenerHelper_FindInfoBySequenceID_should_return_NULL_for_not_found(void)
{
    l->rx_info_max_used = 100;
    struct rx_info_t *info = &l->rx_info[95];
    info->state = RIS_HOLD;
    info->u.hold.fd = 75;
    info->u.hold.seq_id = 12345;
    TEST_ASSERT_EQUAL(NULL, ListenerHelper_FindInfoBySequenceID(l, 75, 12346));
    TEST_ASSERT_EQUAL(NULL, ListenerHelper_FindInfoBySequenceID(l, 74, 12345));
}
