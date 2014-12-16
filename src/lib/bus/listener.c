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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include <poll.h>
#include <err.h>

#include "bus_internal_types.h"
#include "listener.h"
#include "listener_internal.h"
#include "util.h"
#include "atomic.h"

#define DEFAULT_READ_BUF_SIZE (1024L * 1024L)
#define RESPONSE_TIMEOUT 10

static void retry_delivery(listener *l, rx_info_t *info);

struct listener *listener_init(struct bus *b, struct bus_config *cfg) {
    struct listener *l = calloc(1, sizeof(*l));
    if (l == NULL) { return NULL; }

    l->bus = b;
    BUS_LOG(b, 2, LOG_LISTENER, "init", b->udata);

    struct casq *q = casq_new();
    if (q == NULL) {
        free(l);
        return NULL;
    }
    l->q = q;

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        uint8_t *p_id = (uint8_t *)&info->id;
        if (i < MAX_PENDING_MESSAGES - 1) {
            info->next = &l->rx_info[i + 1];
        }
        *p_id = i;
    }
    l->rx_info_freelist = &l->rx_info[0];
    l->info_available = MAX_PENDING_MESSAGES;

    for (int i = 0; i < MAX_QUEUE_MESSAGES; i++) {
        listener_msg *msg = &l->msgs[i];
        uint8_t *p_id = (uint8_t *)&msg->id;
        *p_id = i;
        if (i < MAX_QUEUE_MESSAGES - 1) { /* forward link */
            msg->next = &l->msgs[i + 1];
        }
    }
    l->msg_freelist = &l->msgs[0];

    return l;
}

bool listener_add_socket(struct listener *l,
        connection_info *ci, int notify_fd) {
    listener_msg *msg = get_free_msg(l);
    if (msg == NULL) { return false; }

    msg->type = MSG_ADD_SOCKET;
    msg->u.add_socket.info = ci;
    msg->u.add_socket.notify_fd = notify_fd;

    return push_message(l, msg);
}

bool listener_close_socket(struct listener *l, int fd) {
    listener_msg *msg = get_free_msg(l);
    if (msg == NULL) { return false; }

    msg->type = MSG_CLOSE_SOCKET;
    msg->u.close_socket.fd = fd;

    return push_message(l, msg);
}

/* Coefficients for backpressure based on certain conditions. */
#define MSG_BP_1QTR       (0)
#define MSG_BP_HALF       (0.5)
#define MSG_BP_3QTR       (2.0)
#define RX_INFO_BP_1QTR   (0)
#define RX_INFO_BP_HALF   (0.5)
#define RX_INFO_BP_3QTR   (2.0)
#define THREADPOOL_BP     (1.0)

static uint16_t get_backpressure(struct listener *l) {
    uint16_t msg_fill_pressure = 0;
    
    if (l->msgs_in_use < 0.25 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = 0;
    } else if (l->msgs_in_use < 0.5 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = MSG_BP_1QTR * l->msgs_in_use;
    } else if (l->msgs_in_use < 0.75 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = MSG_BP_HALF * l->msgs_in_use;
    } else {
        msg_fill_pressure = MSG_BP_3QTR * l->msgs_in_use;
    }

    uint16_t rx_info_fill_pressure = 0;
    if (l->rx_info_in_use < 0.25 * MAX_PENDING_MESSAGES) {
        rx_info_fill_pressure = 0;
    } else if (l->rx_info_in_use < 0.5 * MAX_PENDING_MESSAGES) {
        rx_info_fill_pressure = RX_INFO_BP_1QTR * l->rx_info_in_use;
    } else if (l->rx_info_in_use < 0.75 * MAX_PENDING_MESSAGES) {
        rx_info_fill_pressure = RX_INFO_BP_HALF * l->rx_info_in_use;
    } else {
        rx_info_fill_pressure = RX_INFO_BP_3QTR * l->rx_info_in_use;
    }
    
    uint16_t threadpool_fill_pressure = THREADPOOL_BP * l->upstream_backpressure;

    return msg_fill_pressure + rx_info_fill_pressure
      + threadpool_fill_pressure;
}

static void observe_backpressure(listener *l, size_t backpressure) {
    size_t cur = l->upstream_backpressure;
    l->upstream_backpressure = (cur + backpressure) / 2;
}

bool listener_expect_response(struct listener *l, boxed_msg *box,
        uint16_t *backpressure) {
    listener_msg *msg = get_free_msg(l);
    struct bus *b = l->bus;
    if (msg == NULL) { return false; }

    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "listener_expect_response with box of %p", box);

    msg->type = MSG_EXPECT_RESPONSE;
    msg->u.expect.box = box;
    *backpressure = get_backpressure(l);

    return push_message(l, msg);
}

bool listener_shutdown(struct listener *l) {
    listener_msg *msg = get_free_msg(l);
    if (msg == NULL) { return false; }

    msg->type = MSG_SHUTDOWN;
    return push_message(l, msg);
}

static void free_queue_cb(void *data, void *udata) {
    listener_msg *msg = (listener_msg *)data;
    switch (msg->type) {
    case MSG_ADD_SOCKET:
        if (msg->u.add_socket.info) { free_ci(msg->u.add_socket.info); }
        break;
    case MSG_EXPECT_RESPONSE:
        if (msg->u.expect.box) { free(msg->u.expect.box); }
        break;
    default:
        break;
    }
}

void listener_free(struct listener *l) {
    /* assert: pthread must be join'd. */
    if (l) {
        casq_free(l->q, free_queue_cb, l);

        for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
            rx_info_t *info = &l->rx_info[i];
            if (info->box) {
                free(info->box);
                info->box = NULL;
            }
        }

        for (int i = 0; i < l->tracked_fds; i++) {
            /* Forget off the front to stress forget_socket. */
            forget_socket(l, l->fds[0].fd);
        }

        if (l->read_buf) {
            free(l->read_buf);
        }                

        free(l);
    }
}


// ==================================================

#define MAX_TIMEOUT 100

void *listener_mainloop(void *arg) {
    listener *self = (listener *)arg;
    assert(self);
    struct bus *b = self->bus;
    int timeout = 1;

    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    time_t last_sec = tv.tv_sec;

    /* The listener thread has full control over its execution -- the
     * only thing other threads can do is put messages into its
     * thread-safe queue to be processed, so it doesn't need any
     * internal locking. */

    while (!self->shutdown) {
        bool work_done = false;
        gettimeofday(&tv, NULL);
        time_t cur_sec = tv.tv_sec;
        if (cur_sec != last_sec) {
            tick_handler(self);
            last_sec = cur_sec;
        }

        /* Pop queue for incoming events, if able to handle them. */
        listener_msg *msg = casq_pop(self->q);
        while (msg && self->info_available > 0) {
            msg_handler(self, msg);
            listener_msg *nmsg = casq_pop(self->q);
            msg = nmsg;
            timeout = 1;
            work_done = true;
        }

        int res = poll(self->fds, self->tracked_fds, timeout);
        BUS_LOG_SNPRINTF(b, (res == 0 ? 6 : 4), LOG_LISTENER, b->udata, 64,
            "poll res %d", res);

        if (res < 0) {
            if (util_is_resumable_io_error) {
                errno = 0;
            } else {
                /* unrecoverable poll error -- FD count is bad
                 * or FDS is a bad pointer. */
                assert(false);
            }
        } else if (res > 0) {
            attempt_recv(self, res);
            work_done = true;
        } else {
            /* nothing to do */
        }

        if (work_done) {
            timeout = 1;
        } else {
            timeout <<= 1;
            if (timeout > MAX_TIMEOUT) {
                timeout = MAX_TIMEOUT;
            }
        }
    }

    BUS_LOG(b, 3, LOG_LISTENER, "shutting down", b->udata);
    return NULL;
}

static void set_error_for_socket(listener *l, int id, int fd, rx_error_t err) {
    /* Mark all pending messages on this socket as being failed due to error. */

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        if (!info->active) {
            continue;
        }

        struct boxed_msg *box = info->box;
        if (box && box->fd == fd) {
            info->error = err;
        }
    }
    
    l->fds[id].events &= ~POLLIN;
}

static void attempt_recv(listener *l, int available) {
    /*   --> failure --> close socket, don't die */
    struct bus *b = l->bus;
    int read_from = 0;
    BUS_LOG(b, 3, LOG_LISTENER, "attempting receive", b->udata);

    for (int i = 0; i < l->tracked_fds; i++) {
        if (read_from == available) { break; }
        struct pollfd *fd = &l->fds[i];
        connection_info *ci = l->fd_info[i];
        assert(ci->fd == fd->fd);

        if (fd->revents & POLLERR) {
            read_from++;
            BUS_LOG(b, 2, LOG_LISTENER, "pollfd: socket error POLLERR", b->udata);
            set_error_for_socket(l, i, ci->fd, RX_ERROR_POLLERR);
        } else if (fd->revents & POLLHUP) {
            read_from++;
            BUS_LOG(b, 3, LOG_LISTENER, "pollfd: socket error POLLHUP", b->udata);
            set_error_for_socket(l, i, ci->fd, RX_ERROR_POLLHUP);
        } else if (fd->revents & POLLIN) {
            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                "reading %zd bytes from socket (buf is %zd)",
                ci->to_read_size, l->read_buf_size);
            assert(l->read_buf_size >= ci->to_read_size);

            ssize_t size = read(fd->fd, l->read_buf, ci->to_read_size);

            if (size == -1) {
                if (util_is_resumable_io_error(errno)) {
                    errno = 0;
                } else {
                    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                        "read: socket error reading, %d", errno);
                    set_error_for_socket(l, i, ci->fd, RX_ERROR_READ_FAILURE);
                    errno = 0;
                }
            } else if (size > 0) {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "read %zd bytes, calling sink CB", size);

                bus_sink_cb_res_t sres = b->sink_cb(l->read_buf, size, ci->udata);
                if (sres.full_msg_buffer) {
                    BUS_LOG(b, 3, LOG_LISTENER, "calling unpack CB", b->udata);
                    bus_unpack_cb_res_t ures = b->unpack_cb(sres.full_msg_buffer, ci->udata);
                    process_unpacked_message(l, ci, ures);
                }

                ci->to_read_size = sres.next_read;

                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "expecting next read to have %zd bytes", ci->to_read_size);

                /* Grow read buffer if necessary. */
                if (ci->to_read_size > l->read_buf_size) {
                    if (!grow_read_buf(l, ci->to_read_size)) {
                        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                            "Read buffer realloc failure for %p (%zd to %zd)",
                            l->read_buf, l->read_buf_size, ci->to_read_size);
                        assert(false);
                    }
                }
            }
        }
    }
}

static rx_info_t *find_info_by_sequence_id(listener *l,
        int fd, int64_t seq_id) {

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        
        /* If not yet filled out, skip. */
        if (info->timeout_sec == TIMEOUT_NOT_YET_SET) { continue; }
        if (!info->active) { continue; }
        struct boxed_msg *box = info->box;

        struct bus *b = l->bus;
        BUS_LOG_SNPRINTF(b, 4, LOG_MEMORY, b->udata, 128,
            "find_info_by_sequence_id: info (%p) at +%d [s %d]: box is %p",
            info, info->id, info->error, box);
        
        if (box == NULL) {
            continue;
        } else if (box->out_seq_id == seq_id && box->fd == fd) {
            return info;
        }
    }

    /* Not found. Probably an unsolicited status message. */
    return NULL;
}

static void process_unpacked_message(listener *l,
        connection_info *ci, bus_unpack_cb_res_t result) {
    struct bus *b = l->bus;
    size_t backpressure;

    /* NOTE: message may be an unsolicited status message */

    if (result.ok) {
        int64_t seq_id = result.u.success.seq_id;
        void *opaque_msg = result.u.success.msg;

        rx_info_t *info = find_info_by_sequence_id(l, ci->fd, seq_id);
        if (info) {
            struct boxed_msg *box = info->box;
            if (box) {
                info->error = RX_ERROR_READY_FOR_DELIVERY;
                
                BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                    "releasing box %p at line %d", box, __LINE__);
                info->box = NULL;  /* release */
                bus_msg_result_t *result = &box->result;
                result->status = BUS_SEND_SUCCESS;
                result->u.response.seq_id = seq_id;
                result->u.response.opaque_msg = opaque_msg;
                if (bus_process_boxed_message(b, box, &backpressure)) {
                    /* success */
                    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                        "successfully delivered box %p, marking info %p as DONE", box, info);
                    info->error = RX_ERROR_DONE;
                    assert(info->box == NULL);
                } else {
                    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                        "returning box %p at line %d", box, __LINE__);
                    info->box = box; /* retry in tick_handler */
                }

                observe_backpressure(l, backpressure);
            } else {
                assert(false);
            }
        } else {
            /* FIXME: We need to save info for messages that are unexpected and
             *     not flush them out as unsolicited until the queue of messages
             *     is fully processed. Otherwise, they sit in the queue stuck and
             *     slowing things down. */
            
            BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 128,
                "ACHTUNG! Couldn't find info for seq_id %lld, msg %p",
                seq_id, opaque_msg);
   
            if (b->unexpected_msg_cb) {
                b->unexpected_msg_cb(opaque_msg, seq_id, b->udata, ci->udata);
            }
        }
    } else {
        uintptr_t e_id = result.u.error.opaque_error_id;
        BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 128,
            "Got opaque_error_id of %lu (0x%08lx)",
            e_id, e_id);

        /* Timeouts will clean up after it, and user code already knows there was an error. */
    }
};

static void tick_handler(listener *l) {
    struct bus *b = l->bus;

    BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 128,
        "tick... %p: %d of %d msgs in use, %d of %d rx_info in use, %d tracked_fds",
        l, l->msgs_in_use, MAX_QUEUE_MESSAGES,
        l->rx_info_in_use, MAX_PENDING_MESSAGES, l->tracked_fds);

    if (b->log_level > 5 || 0) {  /* dump rx_info table */
        for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
            printf(" -- in_use: %d, info[%d]: timeout %ld, error %d, box %p\n",
                l->rx_info[i].active,
                l->rx_info[i].id, l->rx_info[i].timeout_sec,
                l->rx_info[i].error, l->rx_info[i].box);
        }
    }

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        rx_info_t *info = &l->rx_info[i];
        if (info->active) {
            if (info->timeout_sec == TIMEOUT_NOT_YET_SET) {
                continue;       /* skip */
            } else if (info->error == RX_ERROR_READY_FOR_DELIVERY) {
                BUS_LOG(b, 4, LOG_LISTENER,
                    "retrying RX event delivery", b->udata);
                retry_delivery(l, info);
            } else if (info->error == RX_ERROR_DONE) {
                BUS_LOG_SNPRINTF(b, 4, LOG_LISTENER, b->udata, 64,
                    "cleaning up completed RX event at info %p", info);
                clean_up_completed_info(l, info);
            } else if (info->error != RX_ERROR_NONE) {
                BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 64,
                    "notifying of rx failure -- error %d (info %p)", info->error, info);
                notify_message_failure(l, info, BUS_SEND_RX_FAILURE);
            } else if (info->timeout_sec == 1) {
                BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 64,
                    "notifying of rx failure -- timeout (info %p)", info);
                notify_message_failure(l, info, BUS_SEND_RX_TIMEOUT);
            } else {
                BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 64,
                    "decrementing countdown on info %p [%u]: %ld",
                    info, info->id, info->timeout_sec - 1);
                info->timeout_sec--;
            }
        }
    }
}

static void retry_delivery(listener *l, rx_info_t *info) {
    assert(info->error == RX_ERROR_READY_FOR_DELIVERY);
    assert(info->box);
    struct bus *b = l->bus;

    assert(info->box->result.status == BUS_SEND_SUCCESS);
    struct boxed_msg *box = info->box;
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "releasing box %p at line %d", box, __LINE__);
    info->box = NULL;       /* release */
    size_t backpressure = 0;
    if (bus_process_boxed_message(l->bus, box, &backpressure)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "successfully delivered box %p from info %p at line %d (retry)",
            box, info, __LINE__);
        info->error = RX_ERROR_DONE;
        release_rx_info(l, info);
    } else {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "returning box %p at line %d", box, __LINE__);
        info->box = box;    /* retry in tick_handler */
    }

    observe_backpressure(l, backpressure);
}

static void clean_up_completed_info(listener *l, rx_info_t *info) {
    assert(info->error == RX_ERROR_DONE);
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "info %p, box is %p at line %d", info, info->box, __LINE__);
    size_t backpressure = 0;
    if (info->box) {
        if (info->box->result.status != BUS_SEND_SUCCESS) {
            printf("*** info %d: info->timeout %ld\n",
                info->id, info->timeout_sec);
            printf("    info->error %d\n", info->error);
            printf("    info->box == %p\n", info->box);
            printf("    info->box->result.status == %d\n", info->box->result.status);
            printf("    info->box->fd %d\n", info->box->fd);
            printf("    info->box->out_seq_id %lld\n", info->box->out_seq_id);
            printf("    info->box->out_msg %p\n", info->box->out_msg);

        }
        assert(info->box == NULL);

        assert(info->box->result.status == BUS_SEND_SUCCESS);
        struct boxed_msg *box = info->box;
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "releasing box %p at line %d", box, __LINE__);
        info->box = NULL;       /* release */
        if (bus_process_boxed_message(l->bus, box, &backpressure)) {
            release_rx_info(l, info);
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                "returning box %p at line %d", box, __LINE__);
            info->box = box;    /* retry in tick_handler */
        }
    } else {                    /* already processed, just release it */
        release_rx_info(l, info);
    }

    observe_backpressure(l, backpressure);
}

static void notify_message_failure(listener *l,
        rx_info_t *info, bus_send_status_t status) {
    assert(info->box);
    info->box->result.status = status;
    size_t backpressure;
    struct bus *b = l->bus;
    
    boxed_msg *box = info->box;
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "releasing box %p at line %d", box, __LINE__);
    info->box = NULL;
    if (bus_process_boxed_message(l->bus, box, &backpressure)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "delivered box %p with failure message %d at line %d (info %p)",
            box, status, __LINE__, info);
        info->error = RX_ERROR_DONE;
        release_rx_info(l, info);
    } else {
        info->box = box;
    }

    observe_backpressure(l, backpressure);
}

static rx_info_t *get_free_rx_info(struct listener *l) {
    struct bus *b = l->bus;

    struct rx_info_t *head = l->rx_info_freelist;
    if (head == NULL) {
        BUS_LOG(b, 6, LOG_SENDER, "No rx_info cells left!", b->udata);
        return NULL;
    } else {
        l->rx_info_freelist = head->next;
        head->next = NULL;
        l->info_available--;
        l->rx_info_in_use++;
        BUS_LOG(l->bus, 4, LOG_LISTENER, "reserving RX info", l->bus->udata);
        head->error = RX_ERROR_NONE;
        head->active = true;
        return head;
    }
}

static void release_rx_info(struct listener *l, rx_info_t *info) {
    struct bus *b = l->bus;
    assert(info->error == RX_ERROR_DONE);
    assert(info->box == NULL);
    assert(info->id < MAX_PENDING_MESSAGES);

    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
        "releasing RX info %d", info->id);

    /* Set to no longer active -- it will be moved to the freelist on the next sweep. */
    info->active = false;
    info->error = RX_ERROR_NONE;
    info->next = l->rx_info_freelist;
    l->rx_info_freelist = info;

    l->rx_info_in_use--;
    l->info_available++;
}

static listener_msg *get_free_msg(listener *l) {
    struct bus *b = l->bus;

    BUS_LOG_SNPRINTF(b, 4, LOG_LISTENER, b->udata, 128,
        "get_free_msg -- in use: %d", l->msgs_in_use);

    for (;;) {
        listener_msg *head = l->msg_freelist;
        if (head == NULL) {
            BUS_LOG(b, 3, LOG_LISTENER, "No free messages!", b->udata);
            return NULL;
        } else if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msg_freelist, head, head->next)) {
            for (;;) {
                int16_t miu = l->msgs_in_use;
                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msgs_in_use, miu, miu + 1)) {
                    BUS_LOG(l->bus, 5, LOG_LISTENER, "got free msg", l->bus->udata);
                    return head;
                }
            }
        }
    }
}

static void release_msg(struct listener *l, listener_msg *msg) {
    struct bus *b = l->bus;
    assert(msg->id < MAX_QUEUE_MESSAGES);

    for (;;) {
        listener_msg *fl = l->msg_freelist;
        msg->next = fl;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msg_freelist, fl, msg)) {
            for (;;) {
                int16_t miu = l->msgs_in_use;
                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msgs_in_use, miu, miu - 1)) {
                    assert(miu >= 0);
                    BUS_LOG(b, 3, LOG_LISTENER, "Releasing msg", b->udata);
                    return;
                }
            }
        }
    }
}

static bool push_message(struct listener *l, listener_msg *msg) {
    if (msg == NULL) { return false; }
    struct bus *b = l->bus;
  
    if (casq_push(l->q, msg)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
            "Pushed message -- %p -- of type %d", msg, msg->type);
        /* TODO: write a wake message to a pipe? */
        return true;
    } else {
        BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
            "Failed to pushed message -- %p", msg);
        return false;
    }
}

static void msg_handler(listener *l, listener_msg *pmsg) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
        "Handling message -- %p", pmsg);

    listener_msg msg = *pmsg;
    release_msg(l, pmsg);

    switch (msg.type) {

    case MSG_ADD_SOCKET:
        add_socket(l, msg.u.add_socket.info, msg.u.add_socket.notify_fd);
        break;
    case MSG_CLOSE_SOCKET:
        forget_socket(l, msg.u.close_socket.fd);
        break;
    case MSG_EXPECT_RESPONSE:
        expect_response(l, msg.u.expect.box);
        break;
    case MSG_SHUTDOWN:
        shutdown(l);
        break;

    case MSG_NONE:
    default:
        assert(false);
        break;
    }
}

static void notify_caller(int fd) {
    uint8_t reply_buf[2] = {0x00, 0x00};

    for (;;) {
        ssize_t wres = write(fd, reply_buf, 2);
        if (wres == 2) { break; }
        if (wres == -1) {
            if (errno == EINTR) {
                errno = 0;
            } else {
                err(1, "write");
            }
        }
    }
}

static bool grow_read_buf(listener *l, size_t nsize) {
    uint8_t *nbuf = realloc(l->read_buf, nsize);
    if (nbuf) {
        struct bus *b = l->bus;
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "Read buffer realloc success, %p (%zd) to %p (%zd)",
            l->read_buf, l->read_buf_size,
            nbuf, nsize);
        l->read_buf = nbuf;
        l->read_buf_size = nsize;
        return true;
    } else {
        return false;
    }
}

static void add_socket(listener *l, connection_info *ci, int notify_fd) {
    /* TODO: if epoll, just register with the OS. */
    struct bus *b = l->bus;
    BUS_LOG(b, 3, LOG_LISTENER, "adding socket", b->udata);

    if (l->tracked_fds == MAX_FDS) {
        /* error: full */
        BUS_LOG(b, 3, LOG_LISTENER, "FULL", b->udata);
    }
    for (int i = 0; i < l->tracked_fds; i++) {
        if (l->fds[i].fd == ci->fd) {
            free(ci);
            notify_caller(notify_fd);
            return;             /* already present */
        }
    }

    int id = l->tracked_fds;
    l->fd_info[id] = ci;
    l->fds[id].fd = ci->fd;
    l->fds[id].events = POLLIN;
    l->tracked_fds++;

    /* Prime the pump by sinking 0 bytes and getting a size to expect. */
    bus_sink_cb_res_t sink_res = b->sink_cb(l->read_buf, 0, ci->udata);
    assert(sink_res.full_msg_buffer == NULL);  // should have nothing to handle yet
    ci->to_read_size = sink_res.next_read;

    if (!grow_read_buf(l, ci->to_read_size)) {
        free(ci);
        notify_caller(notify_fd);
        return;             /* alloc failure */
    }

    BUS_LOG(b, 3, LOG_LISTENER, "added socket", b->udata);
    notify_caller(notify_fd);
}

static void free_ci(connection_info *ci) {
    if (ci) {
        free(ci);
    }
}

static void forget_socket(listener *l, int fd) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 128,
        "forgetting socket %d", fd);

    /* don't really close it, just drop info about it in the listener */
    for (int i = 0; i < l->tracked_fds; i++) {
        if (l->fds[i].fd == fd) {
            if (l->tracked_fds > 1) {
                /* Swap pollfd CI and last ones. */
                struct pollfd pfd = l->fds[i];
                l->fds[i] = l->fds[l->tracked_fds - 1];
                l->fds[l->tracked_fds - 1] = pfd;
                connection_info *ci = l->fd_info[i];
                l->fd_info[i] = l->fd_info[l->tracked_fds - 1];
                l->fd_info[l->tracked_fds - 1] = ci;
                free_ci(ci);
            } else {
                free_ci(l->fd_info[i]);
            }
            l->tracked_fds--;
        }
    }
}

static void expect_response(listener *l, boxed_msg *box) {
    struct bus *b = l->bus;
    BUS_LOG(b, 3, LOG_LISTENER, "notifying to expect response", b->udata);

    /* set to POLLIN and associate callback with msg_id */

    rx_info_t *info = get_free_rx_info(l);
    assert(info);
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "Setting info %p (+%d)'s box to %p",
        info, info->id, box);
    assert(info->box == NULL);
    info->box = box;
    info->timeout_sec = RESPONSE_TIMEOUT;
}

static void shutdown(listener *l) {
    l->shutdown = true;
}
