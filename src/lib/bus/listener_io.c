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
#include "listener_io.h"
#include "listener_helper.h"

#include <unistd.h>
#include <assert.h>

#include "listener_task.h"
#include "syscall.h"
#include "util.h"

static ssize_t socket_read_plain(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static ssize_t socket_read_ssl(struct bus *b,
    listener *l, int pfd_i, connection_info *ci);
static bool sink_socket_read(struct bus *b,
    listener *l, connection_info *ci, ssize_t size);
static void print_SSL_error(struct bus *b,
    connection_info *ci, int lvl, const char *prefix);
static void set_error_for_socket(listener *l, int id,
    int fd, rx_error_t err);
static void process_unpacked_message(listener *l,
    connection_info *ci, bus_unpack_cb_res_t result);

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
        
        BUS_LOG_SNPRINTF(b, 1, LOG_LISTENER, b->udata, 64,
            "poll: l->fds[%d]->revents: 0x%04x",  // NOCOMMIT
            i + INCOMING_MSG_PIPE, fd->revents);

        /* If a socket is about to be shut down, we want to get a
         * complete read from it if possible, because it's likely to be
         * an UNSOLICITEDSTATUS message with a reason for the hangup.
         * Only do single reads otherwise, though, otherwise the
         * listener can end up blocking too long handling consecutive
         * reads on a busy connection and causing the incoming command
         * queue to get backed up. */
        bool is_closing = fd->events & (POLLERR | POLLNVAL | POLLHUP);

        if (fd->revents & POLLIN) {
            // Try to read what we can (possibly before hangup)
            ssize_t cur_read = 0;
            size_t to_read = ci->to_read_size;
            do {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "reading %zd bytes from socket (buf is %zd)",
                    ci->to_read_size, l->read_buf_size);
                BUS_ASSERT(b, b->udata, l->read_buf_size >= to_read);
                
                switch (ci->type) {
                case BUS_SOCKET_PLAIN:
                    cur_read = socket_read_plain(b, l, i, ci);
                    break;
                case BUS_SOCKET_SSL:
                    cur_read = socket_read_ssl(b, l, i, ci);
                    break;
                default:
                    BUS_ASSERT(b, b->udata, false);
                }
                // -1: socket error
                // 0: no more to read
            } while (is_closing && cur_read > 0 && ci->to_read_size > 0);
            read_from++;
        }

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
        }
    }
}
    
static ssize_t socket_read_plain(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    ssize_t accum = 0;
    while (ci->to_read_size > 0) {
        ssize_t size = syscall_read(ci->fd, l->read_buf, ci->to_read_size);
        if (size == -1) {
            BUS_LOG_SNPRINTF(b, 6, LOG_LISTENER, b->udata, 64,
                "read: size %zd, errno %d", size, errno);
            if (errno == EAGAIN) {
                errno = 0;
                return accum;
            } else if (Util_IsResumableIOError(errno)) {
                errno = 0;
                continue;
            } else {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "read: socket error reading, %d", errno);
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                errno = 0;
                return -1;
            }
        }
        
        if (size > 0) {
            BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 64,
                "read: %zd", size);
            sink_socket_read(b, l, ci, size);
            accum += size;
        } else {
            return accum;
        }
    }
    return accum;
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

static ssize_t socket_read_ssl(struct bus *b, listener *l, int pfd_i, connection_info *ci) {
    BUS_ASSERT(b, b->udata, ci->ssl);
    ssize_t accum = 0;
    while (ci->to_read_size > 0) {
        // ssize_t pending = SSL_pending(ci->ssl);
        ssize_t size = (ssize_t)syscall_SSL_read(ci->ssl, l->read_buf, ci->to_read_size);
        
        if (size == -1) {
            int reason = syscall_SSL_get_error(ci->ssl, size);
            switch (reason) {
            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: WANT_READ", ci->fd);
                return accum;
                
            case SSL_ERROR_WANT_WRITE:
                BUS_ASSERT(b, b->udata, false);

            case SSL_ERROR_SYSCALL:
            {
                if (errno == 0) {
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL errno 0");
                    BUS_ASSERT(b, b->udata, false);
                } else if (Util_IsResumableIOError(errno)) {
                    errno = 0;
                    continue;
                } else {
                    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                        "SSL_read fd %d: errno %d", ci->fd, errno);
                    print_SSL_error(b, ci, 1, "SSL_ERROR_SYSCALL");
                    set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                    return -1;
                }
                break;
            }
            case SSL_ERROR_ZERO_RETURN:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
                    "SSL_read fd %d: ZERO_RETURN (HUP)", ci->fd);
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_POLLHUP);
                return -1;
            }
            
            default:
                print_SSL_error(b, ci, 1, "SSL_ERROR UNKNOWN");
                set_error_for_socket(l, pfd_i, ci->fd, RX_ERROR_READ_FAILURE);
                BUS_ASSERT(b, b->udata, false);
            }
        } else if (size > 0) {
            sink_socket_read(b, l, ci, size);
            accum += size;
            if ((size_t)accum == ci->to_read_size) { break; }
        } else {
            break;
        }
    }
    return accum;
}

#define DUMP_READ 0

static bool sink_socket_read(struct bus *b,
        listener *l, connection_info *ci, ssize_t size) {
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 64,
        "read %zd bytes, calling sink CB", size);
    
#if DUMP_READ
    printf("\n");
    for (int i = 0; i < size; i++) {
        if (i > 0 && (i & 15) == 0) { printf("\n"); }
        printf("%02x ", l->read_buf[i]);
    }
    printf("\n\n");
#endif
    
    bus_sink_cb_res_t sres = b->sink_cb(l->read_buf, size, ci->udata);
    if (sres.full_msg_buffer) {
        BUS_LOG(b, 3, LOG_LISTENER, "calling unpack CB", b->udata);
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

    for (int i = 0; i <= l->rx_info_max_used; i++) {
        rx_info_t *info = &l->rx_info[i];
        switch (info->state) {
        case RIS_INACTIVE:
            break;
        case RIS_HOLD:
            /* We should set an error on the info, but let the timeout
             * or a pending EXPECT message handle the error. That way,
             * it can be handled via the status callback whenever
             * possible. */
            info->u.hold.error = err;
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

    if (result.ok) {
        int64_t seq_id = result.u.success.seq_id;
        void *opaque_msg = result.u.success.msg;

        rx_info_t *info = ListenerHelper_FindInfoBySequenceID(l, ci->fd, seq_id);

        if (info) {
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
