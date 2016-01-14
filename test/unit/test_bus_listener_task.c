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
#include "listener_task.h"
#include "listener_task_internal.h"
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
#include "mock_listener_cmd.h"

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
    l->shutdown_notify_fd = LISTENER_NO_FD;
    l->is_idle = false;
    l->tracked_fds = 0;
    l->inactive_fds = 0;
    l->read_buf = NULL;
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
    memset(&now, 0, sizeof(now));
    memset(&cur, 0, sizeof(cur));
}

void tearDown(void) {}

void test_ListenerTask_MainLoop_should_notify_caller_on_shutdown(void)
{
    l->shutdown_notify_fd = 12345;
    ListenerCmd_NotifyCaller_Expect(l, 12345);

    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(LISTENER_SHUTDOWN_COMPLETE_FD, l->shutdown_notify_fd);
}

void test_ListenerTask_MainLoop_should_block_when_there_is_nothing_to_do(void)
{
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        -1, 0);

    TEST_ASSERT_EQUAL(false, l->is_idle);
    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(true, l->is_idle);
}

void test_ListenerTask_MainLoop_should_step_timeouts_once_a_second(void)
{
    l->rx_info_max_used = 2;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_HOLD;
    info0->timeout_sec = 2;

    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_EXPECT;
    info1->u.expect.error = RX_ERROR_NONE;
    info1->timeout_sec = 6;
    info1->u.expect.box = box;

    Util_Timestamp_ExpectAndReturn(&now, true, true);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);

    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(1, info0->timeout_sec);
    TEST_ASSERT_EQUAL(5, info1->timeout_sec);
}

void test_ListenerTask_MainLoop_should_expire_timeouts(void)
{
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_HOLD;
    info0->timeout_sec = 1;
    info0->u.hold.has_result = false;

    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_EXPECT;
    info1->u.expect.error = RX_ERROR_NONE;
    info1->timeout_sec = 1;
    info1->u.expect.box = box;

    Util_Timestamp_ExpectAndReturn(&now, true, true);
    Util_Timestamp_ExpectAndReturn(&cur, false, true);
    Util_Timestamp_ExpectAndReturn(&cur, false, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, true);

    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);

    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(BUS_SEND_RX_TIMEOUT, box->result.status);

    TEST_ASSERT_EQUAL(RIS_INACTIVE, info0->state);
    TEST_ASSERT_EQUAL(RIS_INACTIVE, info1->state);
    TEST_ASSERT_EQUAL(0, l->rx_info_max_used);
}

static void unexpected_msg_cb(void *msg,
        int64_t seq_id, void *bus_udata, void *socket_udata) {
    last_msg = msg;
    last_seq_id = seq_id;
    last_bus_udata = bus_udata;
    last_socket_udata = socket_udata;
}

void test_ListenerTask_MainLoop_should_properly_free_HOLD_message_with_payload(void)
{
    static uint8_t held_message[] = "message";
    static uint8_t bus_udata[] = "bus_udata";
    static uint8_t socket_udata[] = "socket_udata";

    l->tracked_fds = 1;
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_HOLD;
    info0->timeout_sec = 1;
    int hold_msg_fd = 123;
    info0->u.hold.fd = hold_msg_fd;
    info0->u.hold.has_result = true;
    info0->u.hold.result.ok = true;
    info0->u.hold.result.u.success.msg = held_message;
    info0->u.hold.result.u.success.seq_id = 54321;

    b->unexpected_msg_cb = unexpected_msg_cb;
    b->udata = bus_udata;
    connection_info ci = { .fd = hold_msg_fd, .udata = socket_udata };

    l->fd_info[0] = &ci;

    Util_Timestamp_ExpectAndReturn(&now, true, true);
    Util_Timestamp_ExpectAndReturn(&cur, false, true);

    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);

    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(BUS_SEND_RX_TIMEOUT, box->result.status);

    TEST_ASSERT_EQUAL(RIS_INACTIVE, info0->state);

    TEST_ASSERT_EQUAL(held_message, last_msg);
    TEST_ASSERT_EQUAL(54321, last_seq_id);
    TEST_ASSERT_EQUAL(bus_udata, last_bus_udata);
    TEST_ASSERT_EQUAL(socket_udata, last_socket_udata);
}

void test_ListenerTask_MainLoop_should_retry_delivery_of_messages_that_are_ready(void)
{
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_EXPECT;
    info0->u.expect.box = box;
    box->result.status = BUS_SEND_SUCCESS;
    info0->u.expect.error = RX_ERROR_READY_FOR_DELIVERY;

    // fail delivery the first retry
    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, false);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(RX_ERROR_READY_FOR_DELIVERY, info0->u.expect.error);

    // successfully deliver
    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, true);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(RIS_INACTIVE, info0->state);
}

void test_ListenerTask_MainLoop_should_retry_and_clean_up_DONE_messages(void)
{
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_EXPECT;
    info0->u.expect.box = box;
    box->result.status = BUS_SEND_SUCCESS;
    info0->u.expect.error = RX_ERROR_DONE;

    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, false);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);

    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, true);

    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(RIS_INACTIVE, info0->state);
}

void test_ListenerTask_MainLoop_should_not_poll_inactive_fds(void)
{
    l->tracked_fds = 1;
    l->inactive_fds = 1;
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_INACTIVE;

    now.tv_sec = -1;   // skip tick_handler
    Util_Timestamp_ExpectAndReturn(&now, true, true);

    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds - l->inactive_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);
}

void test_ListenerTask_MainLoop_should_retry_and_expire_errored_messages(void)
{
    l->tracked_fds = 1;
    l->rx_info_max_used = 1;
    rx_info_t *info0 = &l->rx_info[0];
    info0->state = RIS_EXPECT;
    info0->u.expect.box = box;
    box->result.status = BUS_SEND_SUCCESS;
    info0->u.expect.error = RX_ERROR_POLLHUP;

    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, false);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);

    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    Bus_ProcessBoxedMessage_ExpectAndReturn(l->bus, box, &backpressure, true);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        LISTENER_TASK_TIMEOUT_DELAY, 0);
    ListenerTask_MainLoop((void *)l);
    TEST_ASSERT_EQUAL(false, l->is_idle);
    TEST_ASSERT_EQUAL(RIS_INACTIVE, info0->state);
    TEST_ASSERT_EQUAL(BUS_SEND_RX_FAILURE, box->result.status);
}

void test_ListenerTask_MainLoop_should_check_commands(void) {
    l->is_idle = true;
    poll_res = 1;
    Util_Timestamp_ExpectAndReturn(&cur, true, true);
    syscall_poll_ExpectAndReturn(l->fds, l->tracked_fds + INCOMING_MSG_PIPE,
        -1, poll_res);
    ListenerCmd_CheckIncomingMessages_Expect(l, &poll_res);
    ListenerIO_AttemptRecv_Expect(l, poll_res);

    ListenerTask_MainLoop((void *)l);
}

void test_ListenerTask_ReleaseMsg_should_repool_listener_messages(void)
{
    listener_msg *msg = &l->msgs[1];
    l->msg_freelist = &l->msgs[2];
    l->msgs_in_use = 5;

    ListenerTask_ReleaseMsg(l, msg);
    TEST_ASSERT_EQUAL(msg, l->msg_freelist);
    TEST_ASSERT_EQUAL(&l->msgs[2], msg->next);
    TEST_ASSERT_EQUAL(MSG_NONE, msg->type);
    TEST_ASSERT_EQUAL(4, l->msgs_in_use);
}

/* ListenerTask_ReleaseRXInfo is alreday tested via ListenerTask_MainLoop. */

void test_ListenerTask_GrowReadBuf_should_grow_the_listeners_read_buffer(void)
{
    l->read_buf = calloc(100, sizeof(uint8_t));
    TEST_ASSERT(l->read_buf);
    l->read_buf_size = 100;

    TEST_ASSERT_TRUE(ListenerTask_GrowReadBuf(l, 200));
    TEST_ASSERT_EQUAL(200, l->read_buf_size);
    TEST_ASSERT(l->read_buf);
    memset(&l->read_buf[0], 0xFF, 200);  // write to notify valgrind

    free(l->read_buf);
}

void test_ListenerTask_GrowReadBuf_should_expose_failures(void)
{
    l->read_buf = calloc(100, sizeof(uint8_t));
    uint8_t *orig_read_buf = l->read_buf;
    TEST_ASSERT(l->read_buf);
    l->read_buf_size = 100;

    // force realloc failure
    TEST_ASSERT_FALSE(ListenerTask_GrowReadBuf(l, (size_t)-1));
    TEST_ASSERT_EQUAL(100, l->read_buf_size);
    TEST_ASSERT_EQUAL(orig_read_buf, l->read_buf);
    free(l->read_buf);
}

/* (ListenerTask_AttemptDelivery is already tested via ListenerTask_MainLoop) */
/* (ListenerTask_NotifyMessageFailure is already tested via ListenerTask_MainLoop) */

void test_ListenerTask_GetBackpressure_should_return_backpressure_proportional_to_internal_load(void)
{
    TEST_ASSERT_EQUAL(0, ListenerTask_GetBackpressure(l));

    /* Ensure that backpressure monotonically increases as msgs and RX_INFOs in use increases. */
    int32_t last = 0;
    for (int16_t msgs_in_use = 0; msgs_in_use < MAX_QUEUE_MESSAGES; msgs_in_use++) {
        last = -1;
        for (uint16_t rx_info_in_use = 0; rx_info_in_use < MAX_PENDING_MESSAGES; rx_info_in_use++) {
            l->msgs_in_use = msgs_in_use;
            l->rx_info_in_use = rx_info_in_use;
            uint16_t bp = ListenerTask_GetBackpressure(l);
            int32_t sbp = bp;  // sign-extended backpressure
            TEST_ASSERT(last <= sbp);
            last = sbp;
        }
    }
    TEST_ASSERT(last != 0);
}

void test_ListenerTask_GetBackpressure_should_return_backpressure_increasing_superlinearly_with_load_as_it_approaches_full(void)
{
    uint16_t last = 0;
    l->msgs_in_use = 0;
    for (uint16_t iu = 0.75 * MAX_PENDING_MESSAGES; iu < MAX_PENDING_MESSAGES; iu++) {
        l->rx_info_in_use = iu;
        uint16_t bp = ListenerTask_GetBackpressure(l);
        TEST_ASSERT(bp - last > 0);
        last = bp;
    }
}
