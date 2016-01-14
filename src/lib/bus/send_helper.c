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

#include "send_helper.h"
#include "send_internal.h"

#include "listener.h"
#include "send.h"
#include "syscall.h"
#include "util.h"

#include <assert.h>

static ssize_t write_plain(struct bus *b, boxed_msg *box);
static ssize_t write_ssl(struct bus *b, boxed_msg *box, SSL *ssl);
static bool enqueue_EXPECT_message_to_listener(bus *b, boxed_msg *box);

#ifdef TEST
struct timeval done;
uint16_t backpressure = 0;
#endif

SendHelper_HandleWrite_res SendHelper_HandleWrite(bus *b, boxed_msg *box) {
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
        Send_HandleFailure(b, box, BUS_SEND_TX_FAILURE);
        return SHHW_ERROR;
    } else if (wrsz == 0) {
        /* If the OS set POLLOUT but we can't actually write, then
         * just go back to the poll() loop with no progress.
         * If we busywait on this, something is deeply wrong. */
        BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 128,
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
        #ifndef TEST
        struct timeval done;
        #endif
        if (Util_Timestamp(&done, true)) {
            box->tv_send_done = done;
        } else {
            Send_HandleFailure(b, box, BUS_SEND_TIMESTAMP_ERROR);
            return SHHW_ERROR;
        }

        if (enqueue_EXPECT_message_to_listener(b, box)) {
            return SHHW_DONE;
        } else {
            Send_HandleFailure(b, box, BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER);
            return SHHW_ERROR;
        }
    } else {
        return SHHW_OK;
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
            if (Util_IsResumableIOError(errno)) {
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
                "sent: %zd", wrsz);
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
    (void)fd;
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
                /* We need to write more, but currently cannot write.
                 * Log and go back to the poll() loop. */
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: WANT_WRITE", fd);
                return 0;

            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 0, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: WANT_READ", fd);
                assert(false);  // shouldn't get this; we're writing.
                break;

            case SSL_ERROR_SYSCALL:
            {
                if (Util_IsResumableIOError(errno)) {
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
static bool enqueue_EXPECT_message_to_listener(bus *b, boxed_msg *box) {
    /* Notify listener that it should expect a response to a
     * successfully sent message. */
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 128,
        "telling listener to EXPECT sent response, with box %p, seq_id %lld",
        (void *)box, (long long)box->out_seq_id);

    if (box->result.status == BUS_SEND_UNDEFINED) {
        box->result.status = BUS_SEND_REQUEST_COMPLETE;
    }

    struct listener *l = Bus_GetListenerForSocket(b, box->fd);

    for (int retries = 0; retries < SEND_NOTIFY_LISTENER_RETRIES; retries++) {
        #ifndef TEST
        uint16_t backpressure = 0;
        #endif
        /* If this succeeds, then this thread cannot touch the box anymore. */
        if (Listener_ExpectResponse(l, box, &backpressure)) {
            Bus_BackpressureDelay(b, backpressure,
                LISTENER_EXPECT_BACKPRESSURE_SHIFT);
            return true;
        } else {
            BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                "enqueue_request_sent: failed delivery %d", retries);
            syscall_poll(NULL, 0, SEND_NOTIFY_LISTENER_RETRY_DELAY);
        }
    }

    /* Timeout, will be treated as a TX error */
    return false;
}
