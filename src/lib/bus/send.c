/*
* kinetic-c
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#include "bus.h"
#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener.h"
#include "syscall.h"
#include "util.h"
#include "atomic.h"
#include "yacht.h"
#include "send_helper.h"
#include "send_internal.h"

#ifdef TEST
struct timeval start;
struct timeval now;
struct pollfd fds[1];
size_t backpressure = 0;
int poll_errno = 0;
int write_errno = 0;
#endif

static bool attempt_to_enqueue_HOLD_message_to_listener(struct bus *b,
    int fd, int64_t seq_id, int16_t timeout_sec);
/* Do a blocking send.
 *
 * Returning true indicates that the message has been queued up for
 * delivery, but the request or response may still fail. Those errors
 * are handled by giving an error status code to the callback.
 * Returning false means that the send was rejected outright, and
 * the callback-based error handling will not be used. */
bool send_do_blocking_send(bus *b, boxed_msg *box) {
    /* Note: assumes that all locking and thread-safe seq_id allocation
     * has been handled upstream. If multiple client requests are queued
     * up to go out at the same time, they must go out in monotonic order,
     * with only a single thread writing to the socket at once. */
    assert(b);
    assert(box);

    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
        "doing blocking send of box %p, with <fd:%d, seq_id %lld>, msg[%zd]: %p",
        (void *)box, box->fd, (long long)box->out_seq_id,
        box->out_msg_size, (void *)box->out_msg);
    
    int timeout_msec = box->timeout_sec * 1000;

#ifndef TEST
    struct timeval start;
    struct timeval now;
    struct pollfd fds[1];
#endif
    if (util_timestamp(&start, true)) {
        box->tv_send_start = start;
    } else {
        BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 128,
            "gettimeofday failure: %d", errno);
        return false;
    }

    fds[0].fd = box->fd;
    fds[0].events = POLLOUT;

    /* Notify the listener that we're about to start writing to a drive,
     * because (in rare cases) the response may arrive between finishing
     * the write and the listener processing the notification. In that
     * case, it should hold onto the unrecognized response until the
     * client notifies it (and passes it the callback).
     *
     * This timeout is several extra seconds so that we don't have
     * a window where the HOLD message has timed out, but the
     * EXPECT hasn't, leading to ambiguity about what to do with
     * the response (which may or may not have arrived).
     * */
    if (!attempt_to_enqueue_HOLD_message_to_listener(b,
            box->fd, box->out_seq_id, box->timeout_sec + 5)) {
        return false;
    }
    assert(box->out_sent_size == 0);

    int rem_msec = timeout_msec;

    while (rem_msec > 0) {
        if (util_timestamp(&now, true)) {
            size_t usec_elapsed = (((now.tv_sec - start.tv_sec) * 1000000)
                + (now.tv_usec - start.tv_usec));
            size_t msec_elapsed = usec_elapsed / 1000;

            rem_msec = timeout_msec - msec_elapsed;
        } else {
            /* If gettimeofday fails here, the listener's hold message has
             * already been sent; it will time out later. We need to treat
             * this like a TX failure (including closing the socket) because
             * we don't know what state the connection was left in. */
            BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 128,
                "gettimeofday failure in poll loop: %d", errno);
            send_handle_failure(b, box, BUS_SEND_TX_FAILURE);
            return true;
        }

        #ifdef TEST
        errno = poll_errno;
        #endif
        int res = syscall_poll(fds, 1, rem_msec);
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
            "handle_write: poll res %d", res);
        if (res == -1) {
            if (errno == EINTR || errno == EAGAIN) { /* interrupted/try again */
                errno = 0;
                continue;
            } else {
                send_handle_failure(b, box, BUS_SEND_TX_FAILURE);
                return true;
            }
        } else if (res == 1) {
            short revents = fds[0].revents;
            if (revents & POLLNVAL) {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
                    "do_blocking_send on %d: POLLNVAL => UNREGISTERED_SOCKET", box->fd);
                send_handle_failure(b, box, BUS_SEND_UNREGISTERED_SOCKET);
                return true;
            } else if (revents & (POLLERR | POLLHUP)) {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
                    "do_blocking_send on %d: POLLERR/POLLHUP => TX_FAILURE (%d)",
                    box->fd, revents);
                send_handle_failure(b, box, BUS_SEND_TX_FAILURE);
                return true;
            } else if (revents & POLLOUT) {
                send_helper_handle_write_res hw_res = send_helper_handle_write(b, box);

                BUS_LOG_SNPRINTF(b, 4, LOG_SENDER, b->udata, 256,
                    "send_helper_handle_write res %d", hw_res);

                switch (hw_res) {
                case SHHW_OK:
                    continue;
                case SHHW_DONE:
                    return true;
                case SHHW_ERROR:
                    return true;
                }
            } else {
                BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 256,
                    "match fail %d", revents);
                assert(false);  /* match fail */
            }
        } else if (res == 0) {  /* timeout */
            break;
        }
    }
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
        "do_blocking_send on <fd:%d, seq_id:%lld>: TX_TIMEOUT",
        box->fd, (long long)box->out_seq_id);
    send_handle_failure(b, box, BUS_SEND_TX_TIMEOUT);
    return true;
}

static bool attempt_to_enqueue_HOLD_message_to_listener(struct bus *b,
    int fd, int64_t seq_id, int16_t timeout_sec) {
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 128,
      "telling listener to expect response, with <fd%d, seq_id:%lld>",
        fd, (long long)seq_id);

    struct listener *l = bus_get_listener_for_socket(b, fd);

    const int max_retries = SEND_NOTIFY_LISTENER_RETRIES;
    for (int try = 0; try < max_retries; try++) {
        if (listener_hold_response(l, fd, seq_id, timeout_sec)) {
            return true;
        } else {
            /* Don't apply much backpressure here since the client
             * thread will get it when the message is done sending. */
            syscall_poll(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY);
        }
    }
    return false;
}

void send_handle_failure(struct bus *b, boxed_msg *box, bus_send_status_t status) {
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "send_handle_failure: box %p, <fd:%d, seq_id:%lld>, status %d",
        (void*)box, box->fd, (long long)box->out_seq_id, status);
    BUS_ASSERT(b, b->udata, status != BUS_SEND_UNDEFINED);

    box->result = (bus_msg_result_t){
        .status = status,
    };
    
    #ifndef TEST
    size_t backpressure = 0;
    #endif

    /* Retry until it succeeds. */
    size_t retries = 0;
    for (;;) {
        if (bus_process_boxed_message(b, box, &backpressure)) {
            BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                "deleted box %p", (void*)box);
            bus_backpressure_delay(b, backpressure,
                LISTENER_EXPECT_BACKPRESSURE_SHIFT);
            return;
        } else {
            retries++;
            syscall_poll(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY);
            if (retries > 0 && (retries & 255) == 0) {
                BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 64,
                    "looping on handle_failure retry: %zd", retries);
            }
        }
    }
}
