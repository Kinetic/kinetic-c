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

void setUp(void) {
    b = &B;
    memset(&Listener, 0, sizeof(Listener));
    l = &Listener;
    l->bus = &B;
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        info->state = RIS_INACTIVE;
    }
    box = &Box;
}

void tearDown(void) {}

void test_ListenerIO_AttemptRecv_should_handle_hangups(void) {
    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_HOLD;
    info1->u.hold.fd = 100;  // non-matching FD, should be skipped

    rx_info_t *info2 = &l->rx_info[2];
    info2->state = RIS_HOLD;
    info2->u.hold.fd = 5; // matching FD

    l->fds[0 + INCOMING_MSG_PIPE].fd = 100;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = 0;  // no event

    l->fds[1 + INCOMING_MSG_PIPE].fd = 5;  // match
    l->fds[1 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = POLLHUP;  // hangup

    connection_info ci0 = {
        .fd = 100,
    };
    l->fd_info[0] = &ci0;

    connection_info ci1 = {
        .fd = 5,
    };
    l->fd_info[1] = &ci1;

    l->tracked_fds = 2;
    l->rx_info_max_used = 2;

    ListenerTask_ReleaseRXInfo_Expect(l, info2);
    ListenerIO_AttemptRecv(l, 1);
    
    // should no longer attempt to read socket; timeout will close it
    TEST_ASSERT_EQUAL(0, l->fds[1 + INCOMING_MSG_PIPE].events);
}

void test_ListenerIO_AttemptRecv_should_handle_socket_errors(void) {
    rx_info_t *info1 = &l->rx_info[1];
    info1->state = RIS_HOLD;
    info1->u.hold.fd = 100;  // non-matching FD, should be skipped

    rx_info_t *info2 = &l->rx_info[2];
    info2->state = RIS_HOLD;
    info2->u.hold.fd = 5; // matching FD

    l->fds[0 + INCOMING_MSG_PIPE].fd = 100;
    l->fds[0 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = 0;  // no event

    l->fds[1 + INCOMING_MSG_PIPE].fd = 5;  // match
    l->fds[1 + INCOMING_MSG_PIPE].events = POLLIN;
    l->fds[1 + INCOMING_MSG_PIPE].revents = POLLERR;  // socket error

    connection_info ci0 = {
        .fd = 100,
    };
    l->fd_info[0] = &ci0;

    connection_info ci1 = {
        .fd = 5,
    };
    l->fd_info[1] = &ci1;

    l->tracked_fds = 2;
    l->rx_info_max_used = 2;

    ListenerTask_ReleaseRXInfo_Expect(l, info2);
    ListenerIO_AttemptRecv(l, 1);
    
    // should no longer attempt to read socket; timeout will close it
    TEST_ASSERT_EQUAL(0, l->fds[1 + INCOMING_MSG_PIPE].events);
}
