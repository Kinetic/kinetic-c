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
#include "send_internal.h"

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

    struct timeval start;
    if (util_timestamp(&start, true)) {
        box->tv_send_start = start;
    } else {
        BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 128,
            "gettimeofday failure: %d", errno);
        return false;
    }

    struct pollfd fds[1];
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
    if (!attempt_to_enqueue_sending_request_message_to_listener(b,
            box->fd, box->out_seq_id, box->timeout_sec + 5)) {
        return false;
    }
    assert(box->out_sent_size == 0);

    int rem_msec = timeout_msec;

    while (rem_msec > 0) {
        struct timeval now;
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
            handle_failure(b, box, BUS_SEND_TX_FAILURE);
            return true;
        }

        int res = syscall_poll(fds, 1, rem_msec);
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
            "handle_write: poll res %d", res);
        if (res == -1) {
            if (errno == EINTR || errno == EAGAIN) { /* interrupted/try again */
                errno = 0;
                continue;
            } else {
                handle_failure(b, box, BUS_SEND_TX_FAILURE);
                return true;
            }
        } else if (res == 1) {
            short revents = fds[0].revents;
            if (revents & POLLNVAL) {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
                    "do_blocking_send on %d: POLLNVAL => UNREGISTERED_SOCKET", box->fd);
                handle_failure(b, box, BUS_SEND_UNREGISTERED_SOCKET);
                return true;
            } else if (revents & (POLLERR | POLLHUP)) {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 256,
                    "do_blocking_send on %d: POLLERR/POLLHUP => TX_FAILURE (%d)",
                    box->fd, revents);
                handle_failure(b, box, BUS_SEND_TX_FAILURE);
                return true;
            } else if (revents & POLLOUT) {
                handle_write_res hw_res = handle_write(b, box);

                BUS_LOG_SNPRINTF(b, 4, LOG_SENDER, b->udata, 256,
                    "handle_write res %d", hw_res);

                switch (hw_res) {
                case HW_OK:
                    continue;
                case HW_DONE:
                    return true;
                case HW_ERROR:
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
    handle_failure(b, box, BUS_SEND_TX_TIMEOUT);
    return true;
}

static bool attempt_to_enqueue_sending_request_message_to_listener(struct bus *b,
    int fd, int64_t seq_id, int16_t timeout_sec) {
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 128,
      "telling listener to expect response, with <fd%d, seq_id:%lld>",
        fd, (long long)seq_id);

    struct listener *l = bus_get_listener_for_socket(b, fd);

    int delay = 1;
    const int max_retries = 10;
    for (int try = 0; try < max_retries; try++) {
        if (listener_hold_response(l, fd, seq_id, timeout_sec)) {
            return true;
        } else {
            /* Don't apply much backpressure here since the client
             * thread will get it when the message is done sending. */
            syscall_poll(NULL, 0, delay);
            if (delay < 5) { delay++; }
        }
    }
    return false;
}

static void handle_failure(struct bus *b, boxed_msg *box, bus_send_status_t status) {
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "handle_failure: box %p, <fd:%d, seq_id:%lld>, status %d",
        (void*)box, box->fd, (long long)box->out_seq_id, status);
    
    box->result = (bus_msg_result_t){
        .status = status,
    };
    
    size_t backpressure = 0;

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
            const int delay = 5;
            syscall_poll(NULL, 0, delay);
            if (retries > 0 && (retries & 255) == 0) {
                BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 64,
                    "looping on handle_failure retry: %zd", retries);
            }
        }
    }
}

static ssize_t write_plain(bus *b, boxed_msg *box);
static ssize_t write_ssl(bus *b, boxed_msg *box, SSL *ssl);
static bool enqueue_request_sent_message_to_listener(bus *b, boxed_msg *box);

static handle_write_res handle_write(bus *b, boxed_msg *box) {
    SSL *ssl = box->ssl;
    ssize_t wrsz = 0;

    /* Attempt a single write to the socket. */
    if (ssl == BUS_NO_SSL) {
        wrsz = write_plain(b, box);
    } else {
        assert(ssl);
        wrsz = write_ssl(b, box, ssl);
    }
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "wrote %zd", wrsz);

    if (wrsz == -1) {
        handle_failure(b, box, BUS_SEND_TX_FAILURE);
        return HW_ERROR;
    } else if (wrsz == 0) {
        /* If the OS set POLLOUT but we can't actually write, then
         * just go back to the poll() loop with no progress.
         * If we busywait on this, something is deeply wrong. */
        BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 128,
            "suspicious: wrote %zd bytes to <fd:%d, seq_id%lld>",
            wrsz, box->fd, (long long)box->out_seq_id);
    } else {
        /* Update amount written so far */
        box->out_sent_size += wrsz;
    }

    size_t msg_size = box->out_msg_size;
    size_t sent_size = box->out_sent_size;
    size_t rem = msg_size - sent_size;

    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "wrote %zd, rem is %zd", wrsz, rem);

    if (rem == 0) {             /* check if whole message is written */
        struct timeval tv;
        if (util_timestamp(&tv, true)) {
            box->tv_send_done = tv;
        }

        if (enqueue_request_sent_message_to_listener(b, box)) {
            return HW_DONE;
        } else {
            handle_failure(b, box, BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER);
            return HW_ERROR;
        }
    } else {
        return HW_OK;
    }
}

static ssize_t write_plain(struct bus *b, boxed_msg *box) {
    int fd = box->fd;
    uint8_t *msg = box->out_msg;
    size_t msg_size = box->out_msg_size;
    size_t sent_size = box->out_sent_size;
    size_t rem = msg_size - sent_size;
    
    BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
        "write %p to %d, %zd bytes",
        (void*)&msg[sent_size], fd, rem);

    /* Attempt a single write. ('for' is due to continue-based retry.) */
    for (;;) {
        ssize_t wrsz = syscall_write(fd, &msg[sent_size], rem);
        if (wrsz == -1) {
            if (util_is_resumable_io_error(errno)) {
                errno = 0;
                continue;
            } else {
                /* will notify about closed socket upstream */
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                    "write: socket error writing, %s", strerror(errno));
                errno = 0;
                return -1;
            }
        } else if (wrsz > 0) {
            BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                "sent: %zd\n", wrsz);
            return wrsz;
        } else {
            return 0;
        }
    }
}

static ssize_t write_ssl(struct bus *b, boxed_msg *box, SSL *ssl) {
    uint8_t *msg = box->out_msg;
    size_t msg_size = box->out_msg_size;
    ssize_t rem = msg_size - box->out_sent_size;
    int fd = box->fd;
    ssize_t written = 0;
    assert(rem >= 0);

    while (rem > 0) {
        ssize_t wrsz = syscall_SSL_write(ssl, &msg[box->out_sent_size], rem);
        BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
            "SSL_write: socket %d, write %zd => wrsz %zd",
            fd, rem, wrsz);
        if (wrsz > 0) {
            written += wrsz;
            return written;
        } else if (wrsz < 0) {
            int reason = syscall_SSL_get_error(ssl, wrsz);
            switch (reason) {
            case SSL_ERROR_WANT_WRITE:
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: WANT_WRITE", fd);
                break;
                
            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: WANT_READ", fd);
                assert(false);  // shouldn't get this; we're writing.
                break;
                
            case SSL_ERROR_SYSCALL:
            {
                if (util_is_resumable_io_error(errno)) {
                    errno = 0;
                    /* don't break; we want to retry on EINTR etc. until
                     * we get WANT_WRITE, otherwise poll(2) may not retry
                     * the socket for too long */
                    continue;
                } else {
                    /* will notify about socket error upstream */
                    BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                        "SSL_write on fd %d: SSL_ERROR_SYSCALL -- %s",
                        fd, strerror(errno));
                    errno = 0;                    
                    return -1;
                }
            }
            case SSL_ERROR_SSL:
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: error SSL (wrsz %zd)",
                    box->fd, wrsz);

                /* Log error messages from OpenSSL */
                for (;;) {
                    unsigned long e = ERR_get_error();
                    if (e == 0) { break; }  // no more errors
                    BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 128,
                        "SSL_write error: %s",
                        ERR_error_string(e, NULL));
                }
                return -1;                
            default:
            {
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                    "SSL_write on socket %d: match fail: error %d", box->fd, reason);
                return -1;
            }
            }
        } else {
            /* SSL_write should give SSL_ERROR_WANT_WRITE when unable
             * to write further. */
        }
    }
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "SSL_write: leaving loop, %zd bytes written", written);
    return written;
}

/* Notify the listener that the client has finished sending a message, and
 * transfer all details for handling the response to it. Blocking. */
static bool enqueue_request_sent_message_to_listener(bus *b, boxed_msg *box) {
    /* Notify listener that it should expect a response to a
     * successfully sent message. */
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 128,
        "telling listener to expect sent response, with box %p, seq_id %lld",
        (void *)box, (long long)box->out_seq_id);
    
    struct listener *l = bus_get_listener_for_socket(b, box->fd);

    for (int retries = 0; retries < 10; retries++) {
        uint16_t backpressure = 0;
        /* If this succeeds, then this thread cannot touch the box anymore. */
        if (listener_expect_response(l, box, &backpressure)) {
            bus_backpressure_delay(b, backpressure,
                LISTENER_EXPECT_BACKPRESSURE_SHIFT);
            return true;
        } else {
            BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                "enqueue_request_sent: failed delivery %d", retries);
            syscall_poll(NULL, 0, 10);  // delay
        }
    }

    /* Timeout, will be treated as a TX error */
    return false;
}
