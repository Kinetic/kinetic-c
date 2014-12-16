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
#include <poll.h>
#include <pthread.h>
#include <errno.h>

#include "bus.h"
#include "bus_types.h"
#include "bus_internal_types.h"
#include "listener.h"
#include "casq.h"
#include "util.h"
#include "atomic.h"
#include "yacht.h"
#include "sender_internal.h"

static void set_error_for_socket(sender *s, int fd, tx_error_t error);
static void attempt_to_resend_success(sender *s, tx_info_t *info);
static void notify_message_failure(sender *s, tx_info_t *info, bus_send_status_t status);
static void enqueue_message_to_listener(sender *s, tx_info_t *info);

struct sender *sender_init(struct bus *b, struct bus_config *cfg) {
    struct sender *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }

    s->bus = b;

    if ((8 * sizeof(tx_flag_t)) < MAX_CONCURRENT_SENDS) {
        /* tx_flag_t MUST have >= MAX_CONCURRENT_SENDS bits, since it
         * is used as bitflags to indicate message availability. */
        assert(false);
    }

    int res = pthread_mutex_init(&s->watch_set_mutex, NULL);

    s->fd_hash_table = yacht_init(HASH_TABLE_SIZE2);
    if (s->fd_hash_table == NULL) {
        free(s);
        return NULL;
    }

    if (res != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(res));
        free(s);
        yacht_free(s->fd_hash_table);
        return NULL;
    }

    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        if (0 != pipe(s->pipes[i])) {
            fprintf(stderr, "Error: pipe(2): %s\n", strerror(errno));
            free(s);
            return NULL;
        }

        s->fds[i].fd = -1;
        s->fds[i].events = POLLOUT;

        int *p_id = (int *)(&s->tx_info[i].id);
        *p_id = i;
        s->tx_info[i].timeout_sec = TIMEOUT_NOT_YET_SET;
    }

    BUS_LOG(b, 2, LOG_SENDER, "init success", b->udata);

    return s;
}

bool sender_enqueue_message(struct sender *s,
        boxed_msg *box, int *response_fd) {
    assert(s);
    assert(box);
    assert(response_fd);
    struct bus *b = s->bus;
    BUS_LOG(b, 6, LOG_SENDER, "get_free_tx_info", b->udata);
    tx_info_t *tx_info = get_free_tx_info(s);

    if (tx_info == NULL) {  /* No messages left. */
        BUS_LOG(b, 3, LOG_SENDER, "enqueue: no messages left", b->udata);
        return false;
    } else {
        *response_fd = s->pipes[tx_info->id][0];
        /* Message is queued for delivery once TX info is populated. */
        BUS_LOG(b, 4, LOG_SENDER, "enqueue: messages enqueued", b->udata);
        return populate_tx_info(s, tx_info, box);
    }
}

/* Should the hash table be used to check for active file descriptors?
 * Doing so saves CPU usage when sending several requests on the same
 * FD.
 *
 * TODO: It also appears to mask a condition that leads to the "NULL
 * INFO" warning in attempt_write below. Revisit during hardware stress
 * testing. */
#define USE_HASH_TABLE 1

/* Check if FD is being watched. If info is non-NULL, then ignore that record. */
static bool is_watched(struct sender *s, int fd, tx_info_t *ignore_info) {
#if USE_HASH_TABLE
    if (ignore_info == NULL) {
        return yacht_member(s->fd_hash_table, fd);
    }
#endif
    /* ASSERT: s->watch_set_mutex is locked. */

    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t bit = 1 << i;
        tx_info_t *info = &s->tx_info[i];
        if (s->tx_flags & bit && info->timeout_sec != TIMEOUT_NOT_YET_SET) {
            if (info != ignore_info && info->box && info->box->fd == fd) {
                return true;
            }
        }
    }
    return false;
}

/* Add a file descriptor to the watch set, if not already present.
 * This will only be called via sender_enqueue_message, on the
 * client / bus thread. */
static bool add_fd_to_watch_set(struct sender *s, int fd) {
    /* Lock watch set, to ensure that if any FDs are added or removed,
     * they are done based on the whole state. */
    int res = pthread_mutex_lock(&s->watch_set_mutex);
    if (res != 0) {
        assert(false);
        return false;
    }

    /* If fd isn't in the watch set, set as newest. */
    bool in_use = is_watched(s, fd, NULL);
    if (!in_use) {
        struct bus *b = s->bus;
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
            "adding FD %d to watch set", fd);
        int idx = s->active_fds;
        s->fds[idx].fd = fd;
        s->fds[idx].events |= POLLOUT;
        s->active_fds++;
        if (!yacht_add(s->fd_hash_table, fd)) {
            assert(false);
        }
    }

    if (0 != pthread_mutex_unlock(&s->watch_set_mutex)) {
        assert(false);
        return false;
    }
    return true;
}

/* Remove a file descriptor from the watch set, if no other pending
 * socket events also reference it. This will only be called via
 * tick_handler, as events time out or complete. */
static bool remove_fd_from_watch_set(struct sender *s, tx_info_t *info) {
    int res = pthread_mutex_lock(&s->watch_set_mutex);
    if (res != 0) {
        assert(false);
        return false;
    }

    /* Remove fd from the watch_set if no other active tx_infos use it. */
    bool in_use = is_watched(s, info->fd, info);
    if (!in_use) {
        struct bus *b = s->bus;
        BUS_LOG(b, 3, LOG_SENDER, "removing FD from watch set", b->udata);
        for (int i = 0; i < s->active_fds; i++) {
            if (s->fds[i].fd == info->fd) {
                if (s->active_fds > 1) {
                    s->fds[i].fd = s->fds[s->active_fds - 1].fd;
                }
                s->active_fds--;
                if (!yacht_remove(s->fd_hash_table, info->fd)) {
                    assert(false);
                }
                break;
            }
        }
    }

    if (0 != pthread_mutex_unlock(&s->watch_set_mutex)) {
        assert(false);
        return false;
    }
    return true;
}

static bool populate_tx_info(struct sender *s,
        tx_info_t *info, boxed_msg *box) {
    struct bus *b = s->bus;
    assert(info->box == NULL);
    info->box = box;
    info->fd = box->fd;
    info->retries = 0;

    int fd = info->box->fd;

    if (!add_fd_to_watch_set(s, fd)) {
        BUS_LOG(b, /*3*/0, LOG_SENDER, "add_fd_to_watch_set FAILED", b->udata);
        return false;
    }

    /* Set timeout -- the fuse is lit. */
    info->timeout_sec = TX_TIMEOUT;

    return true;
}

static void release_tx_info(struct sender *s, tx_info_t *info) {
    struct bus *b = s->bus;
    assert(info->id < MAX_CONCURRENT_SENDS);

    /* the box should already be released to the listener or threadpool. */
    assert(info->box == NULL);

    info->timeout_sec = TIMEOUT_NOT_YET_SET;
    info->error = TX_ERROR_NONE;

    remove_fd_from_watch_set(s, info);

    for (;;) {
        tx_flag_t cur = s->tx_flags;
        cur &= ~(1 << info->id);
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->tx_flags, s->tx_flags, cur)) {
            for (;;) {
                BUS_LOG(b, 6, LOG_SENDER, "released TX info", b->udata);

                int16_t tiu = s->tx_info_in_use;
                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->tx_info_in_use, tiu, tiu - 1)) {
                    return;
                }
            }
        }
    }
}

static tx_info_t *get_free_tx_info(struct sender *s) {
    struct bus *b = s->bus;
    const tx_flag_t NO_MSG = (1L << MAX_CONCURRENT_SENDS) - 1;
    if (s->tx_flags == NO_MSG) {
        BUS_LOG(b, /*6*/ 0, LOG_SENDER, "No tx_info cells left!", b->udata);
        return NULL;           /* no messages left */
    }

    /* Use atomic compare-and-swap to attempt to reserve a tx_info_t. */
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = s->tx_flags;
        tx_flag_t bit = (1 << i);
        if ((cur & bit) == 0) {
            cur |= bit;
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->tx_flags, s->tx_flags, cur)) {

                struct bus *b = s->bus;
                BUS_LOG(b, 6, LOG_SENDER, "reserving TX info", b->udata);
                s->tx_info_in_use++;

                /* *info is now reserved. */
                tx_info_t *info = &s->tx_info[i];
                assert(info->id == i);
                /* Note: Intentionally _not_ memsetting info to 0 here,
                 * so as to avoid race condition with timeout. */
                info->error = TX_ERROR_NONE;
                assert(info->box == NULL);
                info->sent_size = 0;
                return info;
            }
        }
    }

    BUS_LOG(b, /*6*/ 0, LOG_SENDER, "No tx_info cells left!", b->udata);
    return NULL;
}

bool sender_shutdown(struct sender *s) {
    s->shutdown = true;
    return true;
}

void *sender_mainloop(void *arg) {
    sender *self = (sender *)arg;
    assert(self);
    struct bus *b = self->bus;

    int delay = 1;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t last_sec = tv.tv_sec;

    BUS_LOG(b, 5, LOG_SENDER, "entering main loop", b->udata);
    while (!self->shutdown) {
        bool work = false;

        gettimeofday(&tv, NULL);
        time_t cur_sec = tv.tv_sec;
        if (cur_sec != last_sec) {
            BUS_LOG(b, 5, LOG_SENDER, "entering tick handler", b->udata);
            tick_handler(self);
            last_sec = cur_sec;
        }

        /* Poll on writeable sockets.
         * Note: Even if epoll(2) or kqueue(2) are available, it still makes
         *     sense to use poll here -- self->active_fds will be small.
         *
         * Any FDs that are added to the group by another thread will
         * appear *after self->fds[self->active_fds], and time-outs will
         * only remove them inside tick_handler above. */
        BUS_LOG(b, 7, LOG_SENDER, "polling", b->udata);
        
        int res = poll(self->fds, self->active_fds, delay);
        BUS_LOG_SNPRINTF(b, (res == 0 ? 6 : 4), LOG_SENDER, b->udata, 64, "poll res %d, active fds %d",
            res, self->active_fds);

        if (res == -1) {
            if (util_is_resumable_io_error(errno)) {
                errno = 0;
            } else {
                /* unrecoverable poll error -- FD count is bad
                 * or FDS is a bad pointer. */
                assert(false);
            }
        } else if (res > 0) {
            attempt_write(self, res);
            work = true;
        }

        if (work) {
            delay = 1;
        } else {
            delay <<= 1;
            if (delay > MAX_TIMEOUT) { delay = MAX_TIMEOUT; }
        }
    }

    BUS_LOG(b, 4, LOG_SENDER, "shutting down", b->udata);

    return NULL;
}

/* Get the last info associated with a socket, because new
 * requests will be added to the front of the array as slots
 * become available. */
static tx_info_t *get_last_info_for_socket(sender *s, int fd) {
    tx_info_t *res = NULL;
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = 1 << i;
        if (s->tx_flags & cur) {
            tx_info_t *info = &s->tx_info[i];
            if (info->timeout_sec == TIMEOUT_NOT_YET_SET) { continue; }
            if (info->fd == fd) {
                res = info;
            }
        }
    }
    return res;
}

/* For every pending tx_info associated with a given socket, set its
 * err_no field so that it will be cleaned up on the next pass. */
static void set_error_for_socket(sender *s, int fd, tx_error_t error) {
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = 1 << i;
        if (s->tx_flags & cur) {
            tx_info_t *info = &s->tx_info[i];
            if (info->timeout_sec == TIMEOUT_NOT_YET_SET) { continue; }
            if (info->box->fd == fd && info->error == 0) {
                info->error = error;
                struct bus *b = s->bus;
                BUS_LOG(b, /*2*/ 1, LOG_SENDER, "setting error on socket", b->udata);
            }
        }
    }
}

static void attempt_write(sender *s, int available) {
    int written = 0;
    struct bus *b = s->bus;
    for (int i = 0; i < s->active_fds; i++) {
        if (written == available) { break; }

        struct pollfd *pfd = &s->fds[i];
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
            "attempting write on %d (revents 0x%08x)", pfd->fd, pfd->revents);

        if (pfd->revents & POLLERR) {
            written++;
            set_error_for_socket(s, pfd->fd, TX_ERROR_POLLERR);
        } else if (pfd->revents & POLLHUP) {
            written++;
            set_error_for_socket(s, pfd->fd, TX_ERROR_POLLHUP);
        } else if (pfd->revents & POLLOUT) {
            written++;
            tx_info_t *info = get_last_info_for_socket(s, pfd->fd);
            if (info == NULL) {
                /* Polling is telling us a socket is writeable and
                 * we haven't realized we no longer care about it yet.
                 * Should not get here. */
                printf(" *** NULL INFO ***\n");
                continue;
            }
            assert(info);
            if (info->error != 0) {
                BUS_LOG_SNPRINTF(b, /*4*/0, LOG_SENDER, b->udata, 64,
                    "socket has failed, let it time out: %d", info->error);
                continue;       /* socket has failed, let it time out */
            }

            assert(info->box);
            uint8_t *msg = info->box->out_msg;
            size_t msg_size = info->box->out_msg_size;
            size_t rem = msg_size - info->sent_size;
            if (rem == 0) {     /* send completed! shouldn't get here */
                assert(false);
            } else {
                BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
                    "writing %zd", rem);
                ssize_t wrsz = write(pfd->fd, &msg[info->sent_size], rem);
                if (wrsz == -1) {
                    if (util_is_resumable_io_error(errno)) {
                        errno = 0;
                    } else {
                        /* close socket */
                        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                            "write: socket error writing, %d", errno);
                        set_error_for_socket(s, pfd->fd, TX_ERROR_WRITE_FAILURE);
                        errno = 0;
                    }
                } else {
                    info->sent_size += wrsz;
                    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64, "wrote %zd", wrsz);
                    if (rem - wrsz == 0) { /* completed! */
                        enqueue_message_to_listener(s, info);
                    }
                }
            }
        }
    }
}

static bool write_backpressure(sender *s, tx_info_t *info, uint16_t bp) {
    uint8_t buf[2];
    buf[0] = (uint8_t)((bp & 0x00FF) >> 0);
    buf[1] = (uint8_t)((bp & 0xFF00) >> 8);
    int pipe_fd = s->pipes[info->id][1];
    ssize_t res = write(pipe_fd, buf, sizeof(buf));

    if (res == -1) {
        /* Note -- if this fails, a timeout upstream will take care of it.
         *     But, under what circumstances can it even fail? */
        errno = 0;
        return false;
    } else if (res == 2) {
        return true;
    } else {
        assert(false);
    }
    return false;
}

static void tick_handler(sender *s) {
    struct bus *b = s->bus;
    BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
        "tick... %p -- %d of %d tx_info in use, %d active FDs\n",
        s, s->tx_info_in_use, MAX_CONCURRENT_SENDS, s->active_fds);

    /* Walk table and expire timeouts & items which have been sent.
     *
     * All expiration of TX_INFO fields happens in here. */
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t bit = (1 << i);
        if (s->tx_flags & bit) {
            tx_info_t *info = &s->tx_info[i];
            if (info->timeout_sec  == TIMEOUT_NOT_YET_SET) {
                /* skip -- in the middle of init on another thread */
                continue;
            } else if (info->error != TX_ERROR_NONE) {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "notifying of tx failure -- error %d", info->error);
                notify_message_failure(s, info, BUS_SEND_TX_FAILURE);
            } else if (info->sent_size == info->box->out_msg_size) {
                BUS_LOG(b, 5, LOG_SENDER, "attempting to re-send success", b->udata);
                attempt_to_resend_success(s, info);
            } else if (info->timeout_sec == 1) {
                BUS_LOG(b, 2, LOG_SENDER, "notifying of tx failure -- timeout", b->udata);
                notify_message_failure(s, info, BUS_SEND_TX_TIMEOUT);
            } else {
                info->timeout_sec--;
                BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                    "decrementing tx_info %d -- %ld (status %d, box %p)",
                    info->id, info->timeout_sec, info->error, info->box);
            }
        }
    }
}

static void enqueue_message_to_listener(sender *s, tx_info_t *info) {
    /* Notify listener that it should expect a response to a
     * successfully sent message. */
    struct listener *l = bus_get_listener_for_socket(s->bus, info->fd);

    uint16_t backpressure = 0;

    struct bus *b = s->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 128,
        "telling listener to expect response, with box %p", info->box);

    struct boxed_msg *box = info->box;
    info->box = NULL;           /* passed on to listener */

    if (listener_expect_response(l, box, &backpressure)) {
        write_backpressure(s, info, backpressure);   /* alert blocked client thread */
        BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 128, "release_tx_info %d", __LINE__);
        release_tx_info(s, info);
    } else {
        BUS_LOG(b, 2, LOG_SENDER, "failed delivery", b->udata);
        info->box = box;    /* return it since we need to keep managing it */
        info->retries++;
        if (info->retries == SENDER_MAX_DELIVERY_RETRIES) {
            notify_message_failure(s, info, BUS_SEND_RX_FAILURE);
            BUS_LOG(b, 2, LOG_SENDER, "failed delivery, several retries", b->udata);
        }
    }
}

/* FIXME: refactor ^ and v because they're largely identical */

static void attempt_to_resend_success(sender *s, tx_info_t *info) {
    struct bus *b = s->bus;
    BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 128, "release_tx_info %d", __LINE__);
    struct listener *l = bus_get_listener_for_socket(s->bus, info->fd);

    struct boxed_msg *box = info->box;
    info->box = NULL;           /* passed on to listener */

    uint16_t backpressure = 0;
    if (listener_expect_response(l, box, &backpressure)) {
        write_backpressure(s, info, backpressure);   /* alert blocked client thread */
        BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 128, "release_tx_info %d", __LINE__);
        release_tx_info(s, info);
    } else {    /* failed to enqueue - retry N times and then cancel */
        BUS_LOG(b, 2, LOG_SENDER, "failed delivery", b->udata);
        info->box = box;    /* return it since we need to keep managing it */
        info->retries++;
        if (info->retries == SENDER_MAX_DELIVERY_RETRIES) {
            notify_message_failure(s, info, BUS_SEND_RX_FAILURE);
            BUS_LOG(b, 2, LOG_SENDER, "failed delivery, several retries", b->udata);
        }
    }
}

static void notify_message_failure(sender *s, tx_info_t *info, bus_send_status_t status) {
    info->box->result.status = status;

    size_t backpressure;
    bus_process_boxed_message(s->bus, info->box, &backpressure);
    info->box = NULL;
    (void)backpressure;
    struct bus *b = s->bus;
    BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 128, "release_tx_info %d", __LINE__);
    release_tx_info(s, info);
}

void sender_free(struct sender *s) {
    if (s) {
        int res = pthread_mutex_destroy(&s->watch_set_mutex);
        /* Must call sender_shutdown and wait for phtread_join first. */
        yacht_free(s->fd_hash_table);
        assert(res == 0);
        free(s);
    }
}
