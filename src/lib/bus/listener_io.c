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
#include "listener_io.h"
#include "listener_io_internal.h"
#include "listener_helper.h"

#include <unistd.h>
#include <assert.h>

#include "listener_task.h"
#include "syscall.h"
#include "util.h"

#define YELP(...) //fprintf(stderr, __VA_ARGS__)

void ListenerIO_AttemptRecv(listener *l, int available) {
    /*   --> failure --> set 'closed' error on socket, don't die */
    struct bus *b = l->bus;
    int read_from = 0;
    BUS_LOG(b, 3, LOG_LISTENER, "attempting receive", b->udata);
    
    for (int i = 0; i < l->tracked_fds; i++) {
        if (read_from == available) { break; }
        struct pollfd *fd = &l->fds[i + INCOMING_MSG_PIPE];
        connection_info *ci = l->fd_info[i];
        BUS_ASSERT(b, b->udata, ci->fd == fd->fd);
        
        /* TODO: handle POLLIN, *then* check for
         * (POLLHUP | POLLERR | POLLNVAL), so if we get a status message
         * with a reason for a hangup we can still pass it along. */

        YELP("checking l->fds[%d]: revents 0x%04x\n", i + 1, l->fds[i + 1].revents); 

        if (fd->revents & (POLLERR | POLLNVAL)) {
            read_from++;
            BUS_LOG(b, 2, LOG_LISTENER,
                "pollfd: socket error (POLLERR | POLLNVAL)", b->udata);
            set_error_for_socket(l, i, ci->fd, RX_ERROR_POLLERR);
        } else if (fd->revents & POLLHUP) {
            read_from++;
            BUS_LOG(b, 3, LOG_LISTENER, "pollfd: socket error POLLHUP",
                b->udata);
            set_error_for_socket(l, i, ci->fd, RX_ERROR_POLLHUP);
        } else if (fd->revents & POLLIN) {
            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                "reading %zd bytes from socket (buf is %zd)",
                ci->to_read_size, l->read_buf_size);
            BUS_ASSERT(b, b->udata, l->read_buf_size >= ci->to_read_size);
            read_from++;

            switch (ci->type) {
            case BUS_SOCKET_PLAIN:
                socket_read_plain(b, l, i, ci);
                break;
            case BUS_SOCKET_SSL:
                socket_read_ssl(b, l, i, ci);
                break;
            default:
                BUS_ASSERT(b, b->udata, false);
            }
        }
    }
}
    
static bool socket_read_plain(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    for (;;) {
        ssize_t size = syscall_read(ci->fd, l->read_buf, ci->to_read_size);
        YELP("read %zd on fd %d, to_read_size %zd\n", size, ci->fd, ci->to_read_size);
        if (size == -1) {
            if (util_is_resumable_io_error(errno)) {
                errno = 0;
                continue;
            } else {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "read: socket error reading, %d", errno);
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                errno = 0;
                return false;
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
}

static void print_SSL_error(struct bus *b, connection_info *ci, int lvl, const char *prefix) {
    unsigned long errval = ERR_get_error();
    char ebuf[256];
    BUS_LOG_SNPRINTF(b, lvl, LOG_LISTENER, b->udata, 64,
        "%s -- ERROR on fd %d -- %s",
        prefix, ci->fd, ERR_error_string(errval, ebuf));
    (void)ci;
    (void)lvl;
    (void)errval;
    (void)ebuf;
    (void)prefix;
}

static bool socket_read_ssl(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    BUS_ASSERT(b, b->udata, ci->ssl);
    while (ci->to_read_size > 0) {
        // ssize_t pending = SSL_pending(ci->ssl);
        ssize_t size = (ssize_t)syscall_SSL_read(ci->ssl, l->read_buf, ci->to_read_size);
        
        if (size == -1) {
            int reason = syscall_SSL_get_error(ci->ssl, size);
            switch (reason) {
            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: WANT_READ\n", ci->fd);
                return true;
                
            case SSL_ERROR_WANT_WRITE:
                BUS_ASSERT(b, b->udata, false);

            case SSL_ERROR_SYSCALL:
            {
                if (errno == 0) {
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL errno 0");
                    BUS_ASSERT(b, b->udata, false);
                } else if (util_is_resumable_io_error(errno)) {
                    errno = 0;
                    continue;
                } else {
                    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                        "SSL_read fd %d: errno %d\n", ci->fd, errno);
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL");
                    set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                    return false;
                }
                break;
            }
            case SSL_ERROR_ZERO_RETURN:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: ZERO_RETURN (HUP)\n", ci->fd);
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_POLLHUP);
                return false;
            }
            
            default:
                print_SSL_error(b, ci, 1, "SSL_ERROR UNKNOWN");
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                BUS_ASSERT(b, b->udata, false);
            }
        } else if (size > 0) {
            sink_socket_read(b, l, ci, size);
            if ((size_t)size == ci->to_read_size) { break; }
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
    
    YELP("sinking read, %zd bytes\n", size);
    bus_sink_cb_res_t sres = b->sink_cb(l->read_buf, size, ci->udata);
    if (sres.full_msg_buffer) {
        BUS_LOG(b, 3, LOG_LISTENER, "calling unpack CB", b->udata);
        YELP("calling unpack_cb\n");
        bus_unpack_cb_res_t ures = b->unpack_cb(sres.full_msg_buffer, ci->udata);
        BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
            "process_unpacked_message: ok? %d, seq_id:%lld",
            ures.ok, (long long)ures.u.success.seq_id);
        process_unpacked_message(l, ci, ures);
    }
    
    ci->to_read_size = sres.next_read;
    
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "expecting next read to have %zd bytes", ci->to_read_size);
    
    /* Grow read buffer if necessary. */
    YELP("grow buffer? %zd => %zd\n", ci->to_read_size, l->read_buf_size);
    if (ci->to_read_size > l->read_buf_size) {
        if (!ListenerTask_GrowReadBuf(l, ci->to_read_size)) {
            BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
                "Read buffer realloc failure for %p (%zd to %zd)",
                l->read_buf, l->read_buf_size, ci->to_read_size);
            BUS_ASSERT(b, b->udata, false);
        }
    }
    return true;
}

static void set_error_for_socket(listener *l, int id, int fd, rx_error_t err) {
    /* Mark all pending messages on this socket as being failed due to error. */
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "set_error_for_socket %d, err %d", fd, err);
    YELP("set_error_for_socket: fd %d, error %d\n", fd, err);

    for (int i = 0; i <= l->rx_info_max_used; i++) {
        rx_info_t *info = &l->rx_info[i];
        switch (info->state) {
        case RIS_INACTIVE:
            break;
        case RIS_HOLD:
            if (info->u.hold.fd == fd) {
                ListenerTask_ReleaseRXInfo(l, info);
            }
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
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                "match fail %d on line %d", info->state, __LINE__);
            BUS_ASSERT(b, b->udata, false);
        }
        }
    }    
    l->fds[id + INCOMING_MSG_PIPE].events &= ~POLLIN;
}

static void process_unpacked_message(listener *l,
        connection_info *ci, bus_unpack_cb_res_t result) {
    struct bus *b = l->bus;

    /* NOTE: message may be an unsolicited status message */

    YELP("processing unpacked message. ok? %d\n", result.ok);

    if (result.ok) {
        int64_t seq_id = result.u.success.seq_id;
        void *opaque_msg = result.u.success.msg;

        YELP("msg: seq_id %lld, opaque_msg %p\n", seq_id, opaque_msg);

        rx_info_t *info = listener_helper_find_info_by_sequence_id(l, ci->fd, seq_id);
        YELP("got: %p (sz %zd)\n", info, sizeof(info));
        YELP("listener_helper_find_info_by_sequence_id <fd:%d, seq_id:%lld> => %p\n",
            ci->fd, seq_id, info);
        if (info) {
            YELP("info->state %d\n", info->state);

            switch (info->state) {
            case RIS_HOLD:
                /* Just save result, to match up later. */
                BUS_ASSERT(b, b->udata, !info->u.hold.has_result);
                info->u.hold.has_result = true;
                info->u.hold.result = result;
                break;
            case RIS_EXPECT:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
                    "marking info %d, seq_id:%lld ready for delivery",
                    info->id, (long long)result.u.success.seq_id);
                info->u.expect.error = RX_ERROR_READY_FOR_DELIVERY;
                BUS_ASSERT(b, b->udata, !info->u.hold.has_result);
                info->u.expect.has_result = true;
                info->u.expect.result = result;
                ListenerTask_AttemptDelivery(l, info);
                break;
            }
            case RIS_INACTIVE:
            default:
                BUS_ASSERT(b, b->udata, false);
            }
        } else {
            /* We received a response that we weren't expecting. */
            if (seq_id != BUS_NO_SEQ_ID) {
                BUS_LOG_SNPRINTF(b, 2 - 2, LOG_LISTENER, b->udata, 128,
                    "Couldn't find info for fd:%d, seq_id:%lld, msg %p",
                    ci->fd, (long long)seq_id, opaque_msg);
            }
            if (b->unexpected_msg_cb) {
                b->unexpected_msg_cb(opaque_msg, seq_id, b->udata, ci->udata);
            }
        }
    } else {
        uintptr_t e_id = result.u.error.opaque_error_id;
        BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 128,
            "Got opaque_error_id of %lu (0x%08lx)",
            e_id, e_id);
        (void)e_id;

        /* Timeouts will clean up after it; give user code a chance to
         * clean up after it here, though technically speaking they
         * could in the unpack_cb above. */
        b->error_cb(result, ci->udata);
    }
}
