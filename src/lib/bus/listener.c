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
#include <time.h>

#include "bus_internal_types.h"
#include "listener.h"
#include "listener_internal.h"
#include "util.h"
#include "atomic.h"

#define DEFAULT_READ_BUF_SIZE (1024L * 1024L)

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
        info->state = RIS_INACTIVE;

        uint16_t *p_id = (uint16_t *)&info->id;
        if (i < MAX_PENDING_MESSAGES - 1) {
            info->next = &l->rx_info[i + 1];
        }
        *p_id = i;
    }
    l->rx_info_freelist = &l->rx_info[0];

    for (int i = 0; i < MAX_QUEUE_MESSAGES; i++) {
        listener_msg *msg = &l->msgs[i];
        uint16_t *p_id = (uint16_t *)&msg->id;
        *p_id = i;
        if (i < MAX_QUEUE_MESSAGES - 1) { /* forward link */
            msg->next = &l->msgs[i + 1];
        }
    }
    l->msg_freelist = &l->msgs[0];
    l->rx_info_max_used = 0;

    (void)cfg;
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

bool listener_remove_socket(struct listener *l, int fd) {
    listener_msg *msg = get_free_msg(l);
    if (msg == NULL) { return false; }

    msg->type = MSG_CLOSE_SOCKET;
    msg->u.close_socket.fd = fd;

    return push_message(l, msg);
}

/* Coefficients for backpressure based on certain conditions. */
#define MSG_BP_1QTR       (0.25)
#define MSG_BP_HALF       (0.5)
#define MSG_BP_3QTR       (2.0)
#define RX_INFO_BP_1QTR   (0.5)
#define RX_INFO_BP_HALF   (0.5)
#define RX_INFO_BP_3QTR   (2.0)
#define THREADPOOL_BP     (1.0)

static uint16_t get_backpressure(struct listener *l) {
    uint16_t msg_fill_pressure = 0;
    
    if (l->msgs_in_use < 0.25 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = 0;
    } else if (l->msgs_in_use < 0.5 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = MSG_BP_1QTR * 2 * l->msgs_in_use;
    } else if (l->msgs_in_use < 0.75 * MAX_QUEUE_MESSAGES) {
        msg_fill_pressure = MSG_BP_HALF * 10 * l->msgs_in_use;
    } else {
        msg_fill_pressure = MSG_BP_3QTR * 100 * l->msgs_in_use;
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

    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
        "lbp: %u, %u (iu %u), %u",
        msg_fill_pressure, rx_info_fill_pressure, l->rx_info_in_use, threadpool_fill_pressure);

    return msg_fill_pressure + rx_info_fill_pressure
      + threadpool_fill_pressure;
}

static void observe_backpressure(listener *l, size_t backpressure) {
    size_t cur = l->upstream_backpressure;
    l->upstream_backpressure = (cur + backpressure) / 2;
}

bool listener_hold_response(struct listener *l, int fd,
        int64_t seq_id, int16_t timeout_sec) {
    listener_msg *msg = get_free_msg(l);
    struct bus *b = l->bus;
    if (msg == NULL) {
        BUS_LOG(b, 1, LOG_LISTENER, "OUT OF MESSAGES", b->udata);
        return false;
    }

    BUS_LOG_SNPRINTF(b, 5, LOG_MEMORY, b->udata, 128,
        "listener_hold_response with fd %d, seq_id %lld",
        fd, seq_id);

    msg->type = MSG_HOLD_RESPONSE;
    msg->u.hold.fd = fd;
    msg->u.hold.seq_id = seq_id;
    msg->u.hold.timeout_sec = timeout_sec;

    bool pm_res = push_message(l, msg);
    if (!pm_res) {
        BUS_LOG_SNPRINTF(b, 0, LOG_MEMORY, b->udata, 128,
            "listener_hold_response with fd %d, seq_id %lld FAILED",
            fd, seq_id);
    }
    return pm_res;
}

bool listener_expect_response(struct listener *l, boxed_msg *box,
        uint16_t *backpressure) {
    listener_msg *msg = get_free_msg(l);
    struct bus *b = l->bus;
    if (msg == NULL) {
        BUS_LOG_SNPRINTF(b, 0, LOG_MEMORY, b->udata, 128,
            "! get_free_msg fail %p", (void*)box);
        return false;
    }

    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "listener_expect_response with box of %p", (void*)box);

    msg->type = MSG_EXPECT_RESPONSE;
    msg->u.expect.box = box;
    *backpressure = get_backpressure(l);

    bool pm = push_message(l, msg);
    if (!pm) {
        BUS_LOG_SNPRINTF(b, 0, LOG_MEMORY, b->udata, 128,
            "! push_message fail %p", (void*)box);
    }
    return pm;
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
    (void)udata;
}

void listener_free(struct listener *l) {
    struct bus *b = l->bus;
    /* assert: pthread must be join'd. */
    if (l) {
        casq_free(l->q, free_queue_cb, l);

        for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
            rx_info_t *info = &l->rx_info[i];

            switch (info->state) {
            case RIS_INACTIVE:
                break;
            case RIS_HOLD:
                break;
            case RIS_EXPECT:
                if (info->u.expect.box) {
                    free(info->u.expect.box);
                    info->u.expect.box = NULL;
                }
                break;
            default:
                BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                    "match fail %d on line %d", info->state, __LINE__);
                assert(false);
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

        int res = poll(self->fds, self->tracked_fds, timeout);
        BUS_LOG_SNPRINTF(b, (res == 0 ? 6 : 4), LOG_LISTENER, b->udata, 64,
            "poll res %d", res);

        /* Pop queue for incoming events, if able to handle them. */
        listener_msg *msg = casq_pop(self->q);
        while (msg && self->rx_info_in_use < MAX_PENDING_MESSAGES) {
            msg_handler(self, msg);
            listener_msg *nmsg = casq_pop(self->q);
            msg = nmsg;
            timeout = 0;
            work_done = true;
        }

        if (res < 0) {
            if (util_is_resumable_io_error(errno)) {
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
            timeout = 0;
        } else if (timeout == 0) {
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

#define RX_INFO_MAX_USED i <= l->rx_info_max_used
//#define RX_INFO_MAX_USED i < MAX_PENDING_MESSAGES

static void set_error_for_socket(listener *l, int id, int fd, rx_error_t err) {
    /* Mark all pending messages on this socket as being failed due to error. */
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "set_error_for_socket %d, err %d", fd, err);

    for (int i = 0; RX_INFO_MAX_USED; i++) {
        rx_info_t *info = &l->rx_info[i];
        switch (info->state) {
        case RIS_INACTIVE:
            break;
        case RIS_HOLD:
            release_rx_info(l, info);
            break;
        case RIS_EXPECT:
        {
            struct boxed_msg *box = info->u.expect.box;
            if (box && box->fd == fd) {
                info->u.expect.error = err;
            }
            break;
        }
        default:
        {
            struct bus *b = l->bus;
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                "match fail %d on line %d", info->state, __LINE__);
            assert(false);
        }
        }
    }    
    l->fds[id].events &= ~POLLIN;
}

static void print_SSL_error(struct bus *b, connection_info *ci, int lvl, const char *prefix) {
    unsigned long errval = ERR_get_error();
    char ebuf[256];
    BUS_LOG_SNPRINTF(b, lvl, LOG_LISTENER, b->udata, 64,
        "%s -- ERROR on fd %d -- %s",
        prefix, ci->fd, ERR_error_string(errval, ebuf));
}

static bool socket_read_plain(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static bool socket_read_ssl(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static bool sink_socket_read(struct bus *b,
    listener *l, connection_info *ci, ssize_t size);

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
        
        if (fd->revents & (POLLERR | POLLNVAL)) {
            read_from++;
            BUS_LOG(b, 2, LOG_LISTENER, "pollfd: socket error (POLLERR | POLLNVAL)", b->udata);
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
            read_from++;

            switch (ci->type) {
            case BUS_SOCKET_PLAIN:
                socket_read_plain(b, l, i, ci);
                break;
            case BUS_SOCKET_SSL:
                socket_read_ssl(b, l, i, ci);
                break;
            default:
                assert(false);
            }
        }
    }
}
    
static bool socket_read_plain(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    ssize_t size = read(ci->fd, l->read_buf, ci->to_read_size);
    if (size == -1) {
        if (util_is_resumable_io_error(errno)) {
            errno = 0;
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                "read: socket error reading, %d", errno);
            set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
            errno = 0;
        }
    }
    
    if (size > 0) {
        BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 64,
            "read: %zd", size);
        
        return sink_socket_read(b, l, ci, size);
    } else {
        return false;
    }
}

static bool socket_read_ssl(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    assert(ci->ssl);
    for (;;) {
        ssize_t pending = SSL_pending(ci->ssl);
        ssize_t size = (ssize_t)SSL_read(ci->ssl, l->read_buf, ci->to_read_size);
        fprintf(stderr, "=== PENDING: %zd, got %zd ===\n", pending, size);
        
        if (size == -1) {
            int reason = SSL_get_error(ci->ssl, size);
            switch (reason) {
            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: WANT_READ\n", ci->fd);
                return true;
                
            case SSL_ERROR_WANT_WRITE:
                assert(false);
                
            case SSL_ERROR_SYSCALL:
            {
                if (errno == 0) {
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL errno 0");
                    assert(false);
                } else if (util_is_resumable_io_error(errno)) {
                errno = 0;
                } else {
                    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                        "SSL_read fd %d: errno %d\n", ci->fd, errno);
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL");
                    set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                }
                break;
            }
            case SSL_ERROR_ZERO_RETURN:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: ZERO_RETURN (HUP)\n", ci->fd);
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_POLLHUP);
                break;
            }
            
            default:
                print_SSL_error(b, ci, 1, "SSL_ERROR UNKNOWN");
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                assert(false);
            }
        } else if (size > 0) {
            sink_socket_read(b, l, ci, size);
        }
    }
    return true;
}

#define DUMP_READ 0

static bool sink_socket_read(struct bus *b,
        listener *l, connection_info *ci, ssize_t size) {
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "read %zd bytes, calling sink CB", size);
    
#if DUMP_READ
    bus_lock_log(b);
    printf("\n");
    for (int i = 0; i < size; i++) {
        if (i > 0 && (i & 15) == 0) { printf("\n"); }
        printf("%02x ", l->read_buf[i]);
    }
    printf("\n\n");
    bus_unlock_log(b);
#endif
    
    bus_sink_cb_res_t sres = b->sink_cb(l->read_buf, size, ci->udata);
    if (sres.full_msg_buffer) {
        BUS_LOG(b, 3, LOG_LISTENER, "calling unpack CB", b->udata);
        bus_unpack_cb_res_t ures = b->unpack_cb(sres.full_msg_buffer, ci->udata);
        BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
            "process_unpacked_message: ok? %d, seq_id %lld", ures.ok, ures.u.success.seq_id);
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
    return true;
}

static rx_info_t *find_info_by_sequence_id(listener *l,
        int fd, int64_t seq_id) {
    struct bus *b = l->bus;    
    for (int i = 0; RX_INFO_MAX_USED; i++) {
        rx_info_t *info = &l->rx_info[i];

        switch (info->state) {
        case RIS_INACTIVE:
            break;            /* skip */
        case RIS_HOLD:
            BUS_LOG_SNPRINTF(b, 4, LOG_MEMORY, b->udata, 128,
                "find_info_by_sequence_id: info (%p) at +%d: fd %d, seq_id %lld",
                (void*)info, info->id, fd, seq_id);
            if (info->u.hold.fd == fd && info->u.hold.seq_id == seq_id) {
                return info;
            }
            break;
        case RIS_EXPECT:
        {
            struct boxed_msg *box = info->u.expect.box;
            BUS_LOG_SNPRINTF(b, 4, LOG_MEMORY, b->udata, 128,
                "find_info_by_sequence_id: info (%p) at +%d [s %d]: box is %p",
                (void*)info, info->id, info->u.expect.error, (void*)box);
            if (box != NULL && box->out_seq_id == seq_id && box->fd == fd) {
                return info;
            }
            break;
        }
        default:
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                "match fail %d on line %d", info->state, __LINE__);
            assert(false);
        }
    }

    if (b->log_level > 5 || 1) {
        BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
            "==== Could not find <fd: %d, seq_id: %lld>, dumping table ====\n", 
            fd, seq_id);
        dump_rx_info_table(l);
    }
    /* Not found. Probably an unsolicited status message. */
    return NULL;
}

static void process_unpacked_message(listener *l,
        connection_info *ci, bus_unpack_cb_res_t result) {
    struct bus *b = l->bus;

    /* NOTE: message may be an unsolicited status message */

    if (result.ok) {
        int64_t seq_id = result.u.success.seq_id;
        void *opaque_msg = result.u.success.msg;

        rx_info_t *info = find_info_by_sequence_id(l, ci->fd, seq_id);
        if (info) {
            switch (info->state) {
            case RIS_HOLD:
                /* Just save result, to match up later. */
                assert(!info->u.hold.has_result);
                info->u.hold.has_result = true;
                info->u.hold.result = result;
                break;
            case RIS_EXPECT:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
                    "marking info %d, seq_id %lld ready for delivery",
                    info->id, result.u.success.seq_id);
                info->u.expect.error = RX_ERROR_READY_FOR_DELIVERY;
                assert(!info->u.hold.has_result);
                info->u.expect.has_result = true;
                info->u.expect.result = result;
                attempt_delivery(l, info);
                break;
            }
            case RIS_INACTIVE:
            default:
                assert(false);
            }
        } else {
            /* We received a response that we weren't expecting. */
            BUS_LOG_SNPRINTF(b, 2 - 2, LOG_LISTENER, b->udata, 128,
                "Couldn't find info for fd %d, seq_id %lld, msg %p",
                ci->fd, (long long)seq_id, opaque_msg);
   
            if (b->unexpected_msg_cb) {
                b->unexpected_msg_cb(opaque_msg, seq_id, b->udata, ci->udata);
            }
        }
    } else {
        uintptr_t e_id = result.u.error.opaque_error_id;
        BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 128,
            "Got opaque_error_id of %lu (0x%08lx)",
            e_id, e_id);

        /* Timeouts will clean up after it; give user code a chance to
         * clean up after it here, though technically speaking they
         * could in the unpack_cb above. */
        b->error_cb(result, ci->udata);
    }
}

static void tick_handler(listener *l) {
    struct bus *b = l->bus;

    BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 128,
        "tick... %p: %d of %d msgs in use, %d of %d rx_info in use, %d tracked_fds",
        (void*)l, l->msgs_in_use, MAX_QUEUE_MESSAGES,
        l->rx_info_in_use, MAX_PENDING_MESSAGES, l->tracked_fds);

    if (b->log_level > 5 || l->msgs_in_use > 5) {  /* dump msg table types */
        for (int i = 0; i < l->msgs_in_use; i++) {
            printf(" -- msg %d: type %d\n", i, l->msgs[i].type);
        }
    }
    
    if (b->log_level > 5 || 0) { dump_rx_info_table(l); }

    for (int i = 0; RX_INFO_MAX_USED; i++) {
        rx_info_t *info = &l->rx_info[i];

        switch (info->state) {
        case RIS_INACTIVE:
            break;
        case RIS_HOLD:
            /* Check timeout */
            if (info->timeout_sec == 1) {
                /* never got a response, but we don't have the callback
                 * either -- the sender will notify about the timeout. */
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "timing out hold info %p", (void*)info);
                release_rx_info(l, info);
            } else {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "decrementing countdown on info %p [%u]: %ld",
                    (void*)info, info->id, info->timeout_sec - 1);
                info->timeout_sec--;
            }
            break;
        case RIS_EXPECT:
            if (info->u.expect.error == RX_ERROR_READY_FOR_DELIVERY) {
                BUS_LOG(b, 4, LOG_LISTENER,
                    "retrying RX event delivery", b->udata);
                retry_delivery(l, info);
            } else if (info->u.expect.error == RX_ERROR_DONE) {
                BUS_LOG_SNPRINTF(b, 4, LOG_LISTENER, b->udata, 64,
                    "cleaning up completed RX event at info %p", (void*)info);
                clean_up_completed_info(l, info);
            } else if (info->u.expect.error != RX_ERROR_NONE) {
                BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 64,
                    "notifying of rx failure -- error %d (info %p)",
                    info->u.expect.error, (void*)info);
                notify_message_failure(l, info, BUS_SEND_RX_FAILURE);
            } else if (info->timeout_sec == 1) {
                BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 64,
                    "notifying of rx failure -- timeout (info %p)", (void*)info);
                notify_message_failure(l, info, BUS_SEND_RX_TIMEOUT);
            } else {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "decrementing countdown on info %p [%u]: %ld",
                    (void*)info, info->id, info->timeout_sec - 1);
                info->timeout_sec--;
            }
            break;
        default:
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                "match fail %d on line %d", info->state, __LINE__);
            assert(false);
        }
    }
}

static void dump_rx_info_table(listener *l) {
    for (int i = 0; RX_INFO_MAX_USED; i++) {
        rx_info_t *info = &l->rx_info[i];
        
        printf(" -- state: %d, info[%d]: timeout %ld",
            info->state, info->id, info->timeout_sec);
        switch (l->rx_info[i].state) {
        case RIS_HOLD:
            printf(", fd %d, seq_id %lld, has_result? %d\n",
                info->u.hold.fd, info->u.hold.seq_id, info->u.hold.has_result);
            break;
        case RIS_EXPECT:
        {
            struct boxed_msg *box = info->u.expect.box;
            printf(", box %p (fd:%d, seq_id:%lld), error %d, has_result? %d\n",
                (void *)box, box ? box->fd : -1, box ? box->out_seq_id : -1,
                info->u.expect.error, info->u.expect.has_result);
            break;
        }
        case RIS_INACTIVE:
            printf(", INACTIVE (next: %d)\n", info->next ? info->next->id : -1);
            break;
        }
    }
}

static void retry_delivery(listener *l, rx_info_t *info) {
    assert(info->state == RIS_EXPECT);
    assert(info->u.expect.error == RX_ERROR_READY_FOR_DELIVERY);
    assert(info->u.expect.box);
    struct bus *b = l->bus;

    struct boxed_msg *box = info->u.expect.box;
    info->u.expect.box = NULL;       /* release */
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "releasing box %p at line %d", (void*)box, __LINE__);
    assert(box->result.status == BUS_SEND_SUCCESS);

    size_t backpressure = 0;
    if (bus_process_boxed_message(l->bus, box, &backpressure)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "successfully delivered box %p (seq_id %lld) from info %d at line %d (retry)",
            (void*)box, box->out_seq_id, info->id, __LINE__);
        info->u.expect.error = RX_ERROR_DONE;
        release_rx_info(l, info);
    } else {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "returning box %p at line %d", (void*)box, __LINE__);
        info->u.expect.box = box;    /* retry in tick_handler */
    }

    observe_backpressure(l, backpressure);
}

static void clean_up_completed_info(listener *l, rx_info_t *info) {
    assert(info->state == RIS_EXPECT);
    assert(info->u.expect.error == RX_ERROR_DONE);
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "info %p, box is %p at line %d", (void*)info,
        (void*)info->u.expect.box, __LINE__);
    size_t backpressure = 0;
    if (info->u.expect.box) {
        struct boxed_msg *box = info->u.expect.box;
        if (box->result.status != BUS_SEND_SUCCESS) {
            printf("*** info %d: info->timeout %ld\n",
                info->id, info->timeout_sec);
            printf("    info->error %d\n", info->u.expect.error);
            printf("    info->box == %p\n", (void*)box);
            printf("    info->box->result.status == %d\n", box->result.status);
            printf("    info->box->fd %d\n", box->fd);
            printf("    info->box->out_seq_id %lld\n", (long long)box->out_seq_id);
            printf("    info->box->out_msg %p\n", (void*)box->out_msg);

        }
        assert(box->result.status == BUS_SEND_SUCCESS);
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "releasing box %p at line %d", (void*)box, __LINE__);
        info->u.expect.box = NULL;       /* release */
        if (bus_process_boxed_message(l->bus, box, &backpressure)) {
            release_rx_info(l, info);
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                "returning box %p at line %d", (void*)box, __LINE__);
            info->u.expect.box = box;    /* retry in tick_handler */
        }
    } else {                    /* already processed, just release it */
        release_rx_info(l, info);
    }

    observe_backpressure(l, backpressure);
}

static void notify_message_failure(listener *l,
        rx_info_t *info, bus_send_status_t status) {
    assert(info->state == RIS_EXPECT);
    assert(info->u.expect.box);
    info->u.expect.box->result.status = status;
    size_t backpressure = 0;
    struct bus *b = l->bus;
    
    boxed_msg *box = info->u.expect.box;
    info->u.expect.box = NULL;
    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "releasing box %p at line %d", (void*)box, __LINE__);
    if (bus_process_boxed_message(l->bus, box, &backpressure)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "delivered box %p with failure message %d at line %d (info %p)",
            (void*)box, status, __LINE__, (void*)info);
        info->u.expect.error = RX_ERROR_DONE;
        release_rx_info(l, info);
    } else {
        /* Return to info, will be released on retry. */
        info->u.expect.box = box;
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
        l->rx_info_in_use++;
        BUS_LOG(l->bus, 4, LOG_LISTENER, "reserving RX info", l->bus->udata);
        assert(head->state == RIS_INACTIVE);
        if (l->rx_info_max_used < head->id) {
            BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
                "rx_info_max_used <- %d", head->id);
            l->rx_info_max_used = head->id;
            assert(l->rx_info_max_used < MAX_PENDING_MESSAGES); 
        }

        BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
            "got free rx_info_t %d (%p)", head->id, (void *)head);
        assert(head == &l->rx_info[head->id]);
        return head;
    }
}

static void release_rx_info(struct listener *l, rx_info_t *info) {
    assert(info);
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
        "releasing RX info %d (%p)", info->id, (void *)info);
    assert(info->id < MAX_PENDING_MESSAGES);
    assert(info == &l->rx_info[info->id]);

    switch (info->state) {
    case RIS_HOLD:
        if (info->u.hold.has_result) {
            /* FIXME: If we have a message that timed out, we need to
             * free it, but don't know howx. We should never get here,
             * because it means the sender finished sending the message,
             * but the listener never got the handler callback. */
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 128,
                "LEAKING RESULT %p", (void *)&info->u.hold.result);
            assert(false);
        }
        break;
    case RIS_EXPECT:
        assert(info->u.expect.error == RX_ERROR_DONE);
        assert(info->u.expect.box == NULL);
        break;
    default:
    case RIS_INACTIVE:
        assert(false);
    }

    /* Set to no longer active and push on the freelist. */
    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
        "releasing rx_info_t %d (%p), was %d",
        info->id, (void *)info, info->state);

    assert(info->state != RIS_INACTIVE);
    info->state = RIS_INACTIVE;
    memset(&info->u, 0, sizeof(info->u));
    info->next = l->rx_info_freelist;
    l->rx_info_freelist = info;

    if (l->rx_info_max_used == info->id && info->id > 0) {
        BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
            "rx_info_max_used--, from %d to %d",
            l->rx_info_max_used, l->rx_info_max_used - 1);
        while (l->rx_info[l->rx_info_max_used].state == RIS_INACTIVE) {
            l->rx_info_max_used--;
            if (l->rx_info_max_used == 0) { break; }
        }
        assert(l->rx_info_max_used < MAX_PENDING_MESSAGES); 
    }

    l->rx_info_in_use--;
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

                    /* Add counterpressure between the sender and the listener.
                     * 10 * ((n >> 1) ** 2) microseconds */
                    int16_t delay = 10 * (miu >> 1) * (miu >> 1);
                    if (delay > 0) {
                        struct timespec ts = {
                            .tv_sec = 0,
                            .tv_nsec = 1000L * delay,
                        };
                        nanosleep(&ts, NULL);
                    }
                    assert(head->type == MSG_NONE);
                    return head;
                }
            }
        }
    }
}

static void release_msg(struct listener *l, listener_msg *msg) {
    struct bus *b = l->bus;
    assert(msg->id < MAX_QUEUE_MESSAGES);
    msg->type = MSG_NONE;

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
    assert(msg);
    struct bus *b = l->bus;
  
    if (casq_push(l->q, msg)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
            "Pushed message -- %p -- of type %d", (void*)msg, msg->type);
        /* TODO: write a wake message to a pipe? */
        return true;
    } else {
        BUS_LOG_SNPRINTF(b, 3 - 3, LOG_LISTENER, b->udata, 128,
            "Failed to pushed message -- %p", (void*)msg);
        assert(false);
        release_msg(l, msg);
        return false;
    }
}

static void msg_handler(listener *l, listener_msg *pmsg) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
        "Handling message -- %p", (void*)pmsg);

    listener_msg msg = *pmsg;
    switch (msg.type) {

    case MSG_ADD_SOCKET:
        add_socket(l, msg.u.add_socket.info, msg.u.add_socket.notify_fd);
        break;
    case MSG_CLOSE_SOCKET:
        forget_socket(l, msg.u.close_socket.fd);
        break;
    case MSG_HOLD_RESPONSE:
        hold_response(l, msg.u.hold.fd, msg.u.hold.seq_id,
            msg.u.hold.timeout_sec);
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
    release_msg(l, pmsg);
}

static void notify_caller(int fd) {
    uint8_t reply_buf[sizeof(uint16_t)] = {0x00};

    for (;;) {
        ssize_t wres = write(fd, reply_buf, sizeof(reply_buf));
        if (wres == sizeof(reply_buf)) { break; }
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
    if (nsize < l->read_buf_size) { return true; }

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
        /* If using SSL, the handle will be freed in the client thread. */
        ci->ssl = NULL;
        free(ci);
    }
}

static void forget_socket(listener *l, int fd) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 128,
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

static void hold_response(listener *l, int fd, int64_t seq_id, int16_t timeout_sec) {
    struct bus *b = l->bus;
    
    rx_info_t *info = get_free_rx_info(l);
    assert(info);
    assert(info->state == RIS_INACTIVE);
    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
        "setting info %p(+%d) to hold response <fd:%d, seq_id:%lld>",
        (void *)info, info->id, fd, seq_id);

    info->state = RIS_HOLD;
    info->timeout_sec = timeout_sec;
    info->u.hold.fd = fd;
    info->u.hold.seq_id = seq_id;
    info->u.hold.has_result = false;
    memset(&info->u.hold.result, 0, sizeof(info->u.hold.result));
}

static rx_info_t *get_hold_rx_info(listener *l, int fd, int64_t seq_id) {
    for (int i = 0; RX_INFO_MAX_USED; i++) {
        rx_info_t *info = &l->rx_info[i];
        if (info->state == RIS_HOLD &&
            info->u.hold.fd == fd &&
            info->u.hold.seq_id == seq_id) {
            return info;
        }
    }
    return NULL;
}

static void attempt_delivery(listener *l, struct rx_info_t *info) {
    struct bus *b = l->bus;

    struct boxed_msg *box = info->u.expect.box;
    info->u.expect.box = NULL;  /* release */
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "attempting delivery of %p", (void*)box);
    BUS_LOG_SNPRINTF(b, 5, LOG_MEMORY, b->udata, 128,
        "releasing box %p at line %d", (void*)box, __LINE__);

    bus_msg_result_t *result = &box->result;
    result->status = BUS_SEND_SUCCESS;

    bus_unpack_cb_res_t unpacked_result;
    switch (info->state) {
    case RIS_EXPECT:
        assert(info->u.expect.has_result);
        unpacked_result = info->u.expect.result;
        break;
    default:
    case RIS_HOLD:
    case RIS_INACTIVE:
        assert(false);
    }

    assert(unpacked_result.ok);
    int64_t seq_id = unpacked_result.u.success.seq_id;
    void *opaque_msg = unpacked_result.u.success.msg;
    result->u.response.seq_id = seq_id;
    result->u.response.opaque_msg = opaque_msg;

    size_t backpressure = 0;
    if (bus_process_boxed_message(b, box, &backpressure)) {
        /* success */
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
            "successfully delivered box %p (seq_id %lld), marking info %d as DONE",
            (void*)box, box->out_seq_id, info->id);
        info->u.expect.error = RX_ERROR_DONE;
        BUS_LOG_SNPRINTF(b, 4, LOG_LISTENER, b->udata, 128,
            "initial clean-up attempt for completed RX event at info +%d", info->id);
        clean_up_completed_info(l, info);
        info = NULL; /* drop out of scope, likely to be stale */
    } else {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "returning box %p at line %d", (void*)box, __LINE__);
        info->u.expect.box = box; /* retry in tick_handler */
    }
    observe_backpressure(l, backpressure);
}

static void expect_response(listener *l, struct boxed_msg *box) {
    assert(box);
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
        "notifying to expect response <box:%p, fd:%d, seq_id:%lld>",
        (void *)box, box->fd, box->out_seq_id);

    /* If there's a pending HOLD message, convert it. */
    rx_info_t *info = get_hold_rx_info(l, box->fd, box->out_seq_id);
    if (info) {
        assert(info->state == RIS_HOLD);
        if (info->u.hold.has_result) {
            bus_unpack_cb_res_t result = info->u.hold.result;

            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 256,
                "converting HOLD to EXPECT for info %d with result, attempting delivery <box:%p, fd:%d, seq_id:%lld>",
                info->id, (void *)box, info->u.hold.fd, info->u.hold.seq_id);

            info->state = RIS_EXPECT;
            info->u.expect.error = RX_ERROR_READY_FOR_DELIVERY;
            info->u.expect.box = box;
            info->u.expect.has_result = true;
            info->u.expect.result = result;
            attempt_delivery(l, info);
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 256,
                "converting HOLD to EXPECT info %d, attempting delivery <box:%p, fd:%d, seq_id:%lld>",
                info->id, (void *)box, info->u.hold.fd, info->u.hold.seq_id);
            info->state = RIS_EXPECT;
            info->u.expect.box = box;
            info->u.expect.error = RX_ERROR_NONE;
            info->u.expect.has_result = false;
            /* Switch over to sender's transferred timeout */
            info->timeout_sec = box->timeout_sec;
        }
    } else {                    /* use free info */
        rx_info_t *info = get_free_rx_info(l);
        assert(info);
        assert(info->state == RIS_INACTIVE);
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
            "Setting info %p (+%d)'s box to %p",
            (void*)info, info->id, (void*)box);
        
        info->state = RIS_EXPECT;
        assert(info->u.expect.box == NULL);
        info->u.expect.box = box;
        info->u.expect.error = RX_ERROR_NONE;
        info->u.expect.has_result = false;
        /* Switch over to sender's transferred timeout */
        info->timeout_sec = box->timeout_sec;
    }
}

static void shutdown(listener *l) {
    l->shutdown = true;
}
