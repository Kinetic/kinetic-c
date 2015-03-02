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
#include <unistd.h>
#include <err.h>
#include <assert.h>
#include <poll.h>

#include "listener_cmd.h"
#include "listener_cmd_internal.h"
#include "listener_task.h"

void ListenerCmd_NotifyCaller(int fd) {
    if (fd == -1) { return; }
    uint8_t reply_buf[sizeof(uint8_t) + sizeof(uint16_t)] = {LISTENER_MSG_TAG};

    /* TODO: reply_buf[1:2] can be little-endian backpressure.  */

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

void ListenerCmd_CheckIncomingMessages(listener *l, int *res) {
    struct bus *b = l->bus;
    short events = l->fds[INCOMING_MSG_PIPE_ID].revents;
    if (events & (POLLERR | POLLHUP | POLLNVAL)) {  /* hangup/error */
        return;
    }

    if (events & POLLIN) {
        char buf[64];
        for (;;) {
            ssize_t rd = read(l->fds[INCOMING_MSG_PIPE_ID].fd, buf, sizeof(buf));
            if (rd == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                } else {
                    BUS_LOG_SNPRINTF(b, 6, LOG_LISTENER, b->udata, 128,
                        "check_and_flush_incoming_msg_pipe: %s", strerror(errno));
                    errno = 0;
                    break;
                }
            } else {
                for (ssize_t i = 0; i < rd; i++) {
                    uint8_t msg_id = buf[i];
                    listener_msg *msg = &l->msgs[msg_id];
                    msg_handler(l, msg);
                }
                (*res)--;
                break;
            }
        }
    }
}

static void msg_handler(listener *l, listener_msg *pmsg) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
        "Handling message -- %p", (void*)pmsg);

    l->is_idle = false;

    listener_msg msg = *pmsg;
    switch (msg.type) {

    case MSG_ADD_SOCKET:
        add_socket(l, msg.u.add_socket.info, msg.u.add_socket.notify_fd);
        break;
    case MSG_REMOVE_SOCKET:
        remove_socket(l, msg.u.remove_socket.fd, msg.u.remove_socket.notify_fd);
        break;
    case MSG_HOLD_RESPONSE:
        hold_response(l, msg.u.hold.fd, msg.u.hold.seq_id,
            msg.u.hold.timeout_sec);
        break;
    case MSG_EXPECT_RESPONSE:
        expect_response(l, msg.u.expect.box);
        break;
    case MSG_SHUTDOWN:
        shutdown(l, msg.u.shutdown.notify_fd);
        break;

    case MSG_NONE:
    default:
        BUS_ASSERT(b, b->udata, false);
        break;
    }
    ListenerTask_ReleaseMsg(l, pmsg);
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
        if (l->fds[i + INCOMING_MSG_PIPE].fd == ci->fd) {
            free(ci);
            ListenerCmd_NotifyCaller(notify_fd);
            return;             /* already present */
        }
    }

    int id = l->tracked_fds;
    l->fd_info[id] = ci;
    l->fds[id + INCOMING_MSG_PIPE].fd = ci->fd;
    l->fds[id + INCOMING_MSG_PIPE].events = POLLIN;
    l->tracked_fds++;

    /* Prime the pump by sinking 0 bytes and getting a size to expect. */
    bus_sink_cb_res_t sink_res = b->sink_cb(l->read_buf, 0, ci->udata);
    BUS_ASSERT(b, b->udata, sink_res.full_msg_buffer == NULL);  // should have nothing to handle yet
    ci->to_read_size = sink_res.next_read;

    //if (!grow_read_buf(l, ci->to_read_size)) {
    if (!ListenerTask_GrowReadBuf(l, ci->to_read_size)) {
        free(ci);
        ListenerCmd_NotifyCaller(notify_fd);
        return;             /* alloc failure */
    }

    BUS_LOG(b, 3, LOG_LISTENER, "added socket", b->udata);
    ListenerCmd_NotifyCaller(notify_fd);
}

static void remove_socket(listener *l, int fd, int notify_fd) {
    struct bus *b = l->bus;
    BUS_LOG_SNPRINTF(b, 2, LOG_LISTENER, b->udata, 128,
        "removing socket %d", fd);

    /* Don't really close it, just drop info about it in the listener.
     * The client thread will actually free the structure, close SSL, etc. */
    for (int i = 0; i < l->tracked_fds; i++) {
        if (l->fds[i + INCOMING_MSG_PIPE].fd == fd) {
            if (l->tracked_fds > 1) {
                /* Swap pollfd CI and last ones. */
                struct pollfd pfd = l->fds[i + INCOMING_MSG_PIPE];
                l->fds[i + INCOMING_MSG_PIPE] = l->fds[l->tracked_fds - 1 + INCOMING_MSG_PIPE];
                l->fds[l->tracked_fds - 1 + INCOMING_MSG_PIPE] = pfd;
                connection_info *ci = l->fd_info[i];
                l->fd_info[i] = l->fd_info[l->tracked_fds - 1];
                l->fd_info[l->tracked_fds - 1] = ci;
            }
            l->tracked_fds--;
        }
    }
    /* CI will be freed by the client thread. */
    ListenerCmd_NotifyCaller(notify_fd);
}

static void hold_response(listener *l, int fd, int64_t seq_id, int16_t timeout_sec) {
    struct bus *b = l->bus;
    
    rx_info_t *info = get_free_rx_info(l);
    if (info == NULL) {
        BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 128,
            "failed to get free rx_info for <fd:%d, seq_id:%lld>, dropping it",
            fd, (long long)seq_id);
        return;
    }
    BUS_ASSERT(b, b->udata, info);
    BUS_ASSERT(b, b->udata, info->state == RIS_INACTIVE);
    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
        "setting info %p(+%d) to hold response <fd:%d, seq_id:%lld>",
        (void *)info, info->id, fd, (long long)seq_id);

    info->state = RIS_HOLD;
    info->timeout_sec = timeout_sec;
    info->u.hold.fd = fd;
    info->u.hold.seq_id = seq_id;
    info->u.hold.has_result = false;
    memset(&info->u.hold.result, 0, sizeof(info->u.hold.result));
}

static rx_info_t *get_hold_rx_info(listener *l, int fd, int64_t seq_id) {
    for (int i = 0; i <= l->rx_info_max_used; i++) {
        rx_info_t *info = &l->rx_info[i];
        if (info->state == RIS_HOLD &&
            info->u.hold.fd == fd &&
            info->u.hold.seq_id == seq_id) {
            return info;
        }
    }
    return NULL;
}

static void expect_response(listener *l, struct boxed_msg *box) {
    struct bus *b = l->bus;
    BUS_ASSERT(b, b->udata, box);
    BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 128,
        "notifying to expect response <box:%p, fd:%d, seq_id:%lld>",
        (void *)box, box->fd, (long long)box->out_seq_id);

    /* If there's a pending HOLD message, convert it. */
    rx_info_t *info = get_hold_rx_info(l, box->fd, box->out_seq_id);
    if (info) {
        BUS_ASSERT(b, b->udata, info->state == RIS_HOLD);
        if (info->u.hold.has_result) {
            bus_unpack_cb_res_t result = info->u.hold.result;

            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 256,
                "converting HOLD to EXPECT for info %d (%p) with result, attempting delivery <box:%p, fd:%d, seq_id:%lld>",
                info->id, (void *)info,
                (void *)box, info->u.hold.fd, (long long)info->u.hold.seq_id);

            info->state = RIS_EXPECT;
            info->u.expect.error = RX_ERROR_READY_FOR_DELIVERY;
            info->u.expect.box = box;
            info->u.expect.has_result = true;
            info->u.expect.result = result;
            ListenerTask_AttemptDelivery(l, info);
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_LISTENER, b->udata, 256,
                "converting HOLD to EXPECT info %d (%p), attempting delivery <box:%p, fd:%d, seq_id:%lld>",
                info->id, (void *)info,
                (void *)box, info->u.hold.fd, (long long)info->u.hold.seq_id);
            info->state = RIS_EXPECT;
            info->u.expect.box = box;
            info->u.expect.error = RX_ERROR_NONE;
            info->u.expect.has_result = false;
            /* Switch over to client's transferred timeout */
            info->timeout_sec = box->timeout_sec;
        }
    } else {                    /* use free info */
        /* If we get here, the listener thinks the HOLD message timed out,
         * but the client doesn't think things timed out badly enough to
         * itself expose an error. We also don't know if we're going to
         * get a response or not. */

        BUS_LOG_SNPRINTF(b, 0, LOG_MEMORY, b->udata, 128,
            "get_hold_rx_info FAILED: fd %d, seq_id %lld",
            box->fd, (long long)box->out_seq_id);

        /* This should be treated like a send timeout. */
        info = get_free_rx_info(l);
        BUS_ASSERT(b, b->udata, info);
        BUS_ASSERT(b, b->udata, info->state == RIS_INACTIVE);

        BUS_LOG_SNPRINTF(b, 0, LOG_MEMORY, b->udata, 256,
            "Setting info %p (+%d)'s box to %p, which will be expired immediately (timeout %lld)",
            (void*)info, info->id, (void*)box, (long long)box->timeout_sec);
        
        info->state = RIS_EXPECT;
        BUS_ASSERT(b, b->udata, info->u.expect.box == NULL);
        info->u.expect.box = box;
        info->u.expect.error = RX_ERROR_NONE;
        info->u.expect.has_result = false;
        /* Switch over to client's transferred timeout */
        ListenerTask_NotifyMessageFailure(l, info, BUS_SEND_RX_TIMEOUT_EXPECT);
    }
}

static void shutdown(listener *l, int notify_fd) {
    l->shutdown_notify_fd = notify_fd;
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
        BUS_ASSERT(b, b->udata, head->state == RIS_INACTIVE);
        if (l->rx_info_max_used < head->id) {
            BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
                "rx_info_max_used <- %d", head->id);
            l->rx_info_max_used = head->id;
            BUS_ASSERT(b, b->udata, l->rx_info_max_used < MAX_PENDING_MESSAGES); 
        }

        BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
            "got free rx_info_t %d (%p)", head->id, (void *)head);
        BUS_ASSERT(b, b->udata, head == &l->rx_info[head->id]);
        return head;
    }
}
