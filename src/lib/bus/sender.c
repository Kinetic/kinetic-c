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

/* Offset for s->fds[0], which is the command pipe. */
#define CMD_FD (1)

struct sender *sender_init(struct bus *b, struct bus_config *cfg) {
    struct sender *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }

    /* TODO: use `goto cleanup` idiom to ensure that all resources are
     *     freed if any intermediate resource acquisition fails. */
    
    s->bus = b;
    
    if ((8 * sizeof(tx_flag_t)) < MAX_CONCURRENT_SENDS) {
        /* tx_flag_t MUST have >= MAX_CONCURRENT_SENDS bits, since it
         * is used as bitflags to indicate message availability. */
        assert(false);
    }
    
    int pipes[2];
    int res = pipe(pipes);
    if (res == -1) {
        free(s);
        return NULL;
    }
    
    s->incoming_command_pipe = pipes[0];
    s->commit_pipe = pipes[1];
    
    s->fd_hash_table = yacht_init(HASH_TABLE_SIZE2);
    if (s->fd_hash_table == NULL) {
        close(pipes[0]);
        close(pipes[1]);
        free(s);
        return NULL;
    }
    
    s->fds[0].fd = s->incoming_command_pipe;
    s->fds[0].events = POLLIN;
    
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        if (0 != pipe(s->pipes[i])) {
            fprintf(stderr, "Error: pipe(2): %s\n", strerror(errno));
            free(s);
            return NULL;
        }
        
        BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
            "PIPE fds %d and %d\n", s->pipes[i][0], s->pipes[i][1]);

        s->fds[i + 1].fd = SENDER_FD_NOT_IN_USE;
        s->fds[i + 1].events = POLLOUT;
        
        int *p_id = (int *)(&s->tx_info[i].id);
        *p_id = i;
        s->tx_info[i].state = TIS_UNDEF;
    }
    
    BUS_LOG(b, 2, LOG_SENDER, "init success", b->udata);
    
    (void)cfg;
    return s;
}

bool sender_register_socket(struct sender *s, int fd, SSL *ssl) {
    struct bus *b = s->bus;
    tx_info_t *info = get_free_tx_info(s);
    if (info == NULL) { return false; }
    
    info->state = TIS_ADD_SOCKET;
    info->u.add_socket.fd = fd;
    info->u.add_socket.ssl = ssl;
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "registering socket %d with SSL %p", fd, (void*)ssl);
    bool res = commit_event_and_block(s, info);
    release_tx_info(s, info);
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "registering socket %d: res %d", fd, res);
    return res;
}

bool sender_remove_socket(struct sender *s, int fd) {
    tx_info_t *info = get_free_tx_info(s);
    if (info == NULL) { return false; }
    
    info->state = TIS_RM_SOCKET;
    info->u.rm_socket.fd = fd;
    bool res = commit_event_and_block(s, info);
    release_tx_info(s, info);
    return res;
}

bool sender_send_request(struct sender *s, boxed_msg *box) {
    struct bus *b = s->bus;
    tx_info_t *info = get_free_tx_info(s);
    if (info == NULL) { return false; }
    
    info->state = TIS_REQUEST_ENQUEUE;
    info->u.enqueue.fd = box->fd;
    info->u.enqueue.box = box;
    
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "sending request on %d: box %p", box->fd, (void*)box);
    bool res = commit_event_and_block(s, info);
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "sending request: releasing tx_info, res %d", res);
    release_tx_info(s, info);
    return res;
}

bool sender_shutdown(struct sender *s) {
    if (s->fd_hash_table == NULL) { return true; }
    struct bus *b = s->bus;
    tx_info_t *info = get_free_tx_info(s);
    if (info == NULL) { return false; }
    
    info->state = TIS_SHUTDOWN;
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "sending shutdown request on %d", info->id);
    bool res = commit_event_and_block(s, info);
    release_tx_info(s, info);
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "shutdown request: %d", res);
    return res;
}

void sender_free(struct sender *s) {
    if (s) {
        cleanup(s);
        free(s);
    }
}

static tx_info_t *get_free_tx_info(struct sender *s) {
    struct bus *b = s->bus;
    /* Use atomic compare-and-swap to attempt to reserve a tx_info_t. */
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = s->tx_flags;
        tx_flag_t bit = (1 << i);
        if ((cur & bit) == 0) {
            tx_flag_t marked = cur | bit;
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->tx_flags, cur, marked)) {
                /* *info is now reserved. */
                tx_info_t *info = &s->tx_info[i];
                BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
                    "reserving free TX info %d", info->id);
                
                assert(info->id == i);
                assert(info->state == TIS_UNDEF);
                info->done_pipe = get_notify_pipe(s, info->id);
                return info;
            }
        }
    }
    
    BUS_LOG(b, 1, LOG_SENDER, "No tx_info cells left", b->udata);
    return NULL;
}

static void release_tx_info(struct sender *s, tx_info_t *info) {
    assert(info->state != TIS_UNDEF);
    info->state = TIS_UNDEF;
    struct bus *b = s->bus;
    assert(info->id < MAX_CONCURRENT_SENDS);
    assert(s->tx_flags & (1 << info->id));
    
    for (;;) {
        tx_flag_t cur = s->tx_flags;
        tx_flag_t cleared = cur & ~(1 << info->id);
        BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
            "CAS 0x%04x => 0x%04x", cur, cleared);
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->tx_flags, cur, cleared)) {
            BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
                "releasing TX info %d", info->id);
            break;
        }
    }
}

static int get_notify_pipe(struct sender *s, int id) {
    return s->pipes[id][0];
}

static bool write_commit(struct sender *s, tx_info_t *info) {
    uint8_t buf[1];
    buf[0] = info->id;
    struct bus *b = s->bus;
    
    for (;;) {
        /* Notify the sender thread that a command is available. */
        ssize_t wr = write(s->commit_pipe, buf, sizeof(buf));
        if (wr == 1) {
            return true;
        } else {
            assert(wr == -1);
            if (errno == EINTR) {
                errno = 0;
            } else {
                BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
                    "write_commit errno %d", errno);
                errno = 0;
                return false;
            }
        }
    }
}

static bool commit_event_and_block(struct sender *s, tx_info_t *info) {
    if (!write_commit(s, info)) { return false; }
    
    struct bus *b = s->bus;
    struct pollfd fds[1];
    assert(info->done_pipe != SENDER_FD_NOT_IN_USE);
    fds[0].fd = info->done_pipe;
    fds[0].events = POLLIN;
    
    for (;;) {
        int res = poll(fds, 1, -1);
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
            "polling done_pipe: %d", res);
        if (res == 1) {
            short ev = fds[0].revents;
            BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 64,
                "poll: ev %d, errno %d", ev, errno);
            if ((ev & POLLHUP) || (ev & POLLERR) || (ev & POLLNVAL)) {
                /* We've been hung up on due to a shutdown event. */
                close(info->done_pipe);
                return true;
            } else if (ev & POLLIN) {
                uint16_t backpressure = 0;
                uint8_t buf[sizeof(bool) + sizeof(backpressure)];
                ssize_t rd = read(info->done_pipe, buf, sizeof(buf));
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "reading done_pipe: %zd", rd);
                if (rd == sizeof(buf)) {
                    bool success = buf[0] == 0 ? false : true;
                    backpressure = (buf[1] << 0);
                    backpressure += (buf[2] << 8);
                    
                    /* Push back if message bus is too busy. */
                    backpressure >>= 7;  // TODO: further tuning
                    
                    if (backpressure > 0) {
                        BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                            "reading done_pipe: backpressure %d", backpressure);
                        poll(NULL, 0, backpressure);
                    }
                    
                    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                        "reading done_pipe: success %d", 1);
                    return success;
                } else if (rd == -1) {
                    if (errno == EINTR) {
                        errno = 0;
                        continue;
                    } else {
                        BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
                            "blocking read on done_pipe: errno %d", errno);
                        errno = 0;
                        return false;
                    }
                }
            } else {
                /* Shouldn't happen -- blocking. */
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                    "shouldn't happen: ev %d, errno %d", ev, errno);
                assert(false);
            }
        } else if (res == -1) {
            BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
                "blocking poll for done_pipe: errno %d", errno);
            errno = 0;
            return false;
        } else {
            /* Shouldn't happen -- blocking. */
            assert(false);
        }
    }
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
            tick_handler(self);  /* handle timeouts and retries */
            last_sec = cur_sec;
        }
        
        /* Poll on incoming command pipe and any sockets for which we have
         * an outgoing message queued up.
         *
         * Note: Even if epoll(2) or kqueue(2) are available, it still makes
         *     sense to use poll here -- self->active_fds will be small. */
        BUS_LOG(b, 7, LOG_SENDER, "polling", b->udata);
        
        int res = poll(self->fds, self->active_fds + CMD_FD, delay);

        BUS_LOG_SNPRINTF(b, (res == 0 ? 6 : 4), LOG_SENDER, b->udata, 64,
            "poll res %d, active fds %d", res, self->active_fds);
        
        if (res == -1) {
            if (util_is_resumable_io_error(errno)) {
                errno = 0;
            } else {
                /* unrecoverable poll error -- FD count is bad
                 * or FDS is a bad pointer. */
                assert(false);
            }
        } else if (res > 0) {
            if (self->fds[0].revents & POLLIN) {
                work = check_incoming_commands(self);
                res--;
            }

            if (self->shutdown) { break; }

            /* If the incoming command pipe isn't the only active FD: */
            if (res > 0) {
                attempt_write(self, res);
            }
            work = true;
        }
        
        if (work) {
            delay = 1;
        } else {
            delay <<= 1;
            if (delay > MAX_TIMEOUT) { delay = MAX_TIMEOUT; }
        }
    }
    
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
        "shutting down, shutdown == %d", self->shutdown);
    cleanup(self);

    return NULL;
}

static void free_fd_info_cb(void *value, void *udata) {
    fd_info *info = (fd_info *)value;
    /* Note: info->ssl will be freed by the listener. */
    (void)udata;
    free(info);
}

static void cleanup(sender *s) {
    struct bus *b = s->bus;
    BUS_LOG(b, 2, LOG_SHUTDOWN, "sender_cleanup", b->udata);
    if (s->fd_hash_table) {     /* make idempotent */
        struct yacht *y = s->fd_hash_table;
        s->fd_hash_table = NULL;
        int shutdown_id = -1;
        
        for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
            if (s->tx_info[i].state == TIS_SHUTDOWN) {
                shutdown_id = i;
            } else {
                close(s->pipes[i][0]);
                close(s->pipes[i][1]);
            }            
        }

        BUS_LOG_SNPRINTF(b, 6 , LOG_SENDER, b->udata, 64,
            "shutdown_id: %d", shutdown_id);
        if (shutdown_id != -1) {
            /* Notify the sender about the shutdown via POLLHUP / POLLNVAL. */
            BUS_LOG(b, 3, LOG_SENDER, "closing to notify shutdown", b->udata);
            /* Client thread will close the other end. */
            close(s->pipes[shutdown_id][1]);
        }
        
        yacht_free(y, free_fd_info_cb, NULL);
    }
}

static bool register_socket_info(sender *s, int fd, SSL *ssl) {
    if (s->shutdown) { return false; }
    fd_info *info = malloc(sizeof(*info));
    if (info == NULL) { 
        return false;
    }
    info->fd = fd;
    info->ssl = ssl;
    info->refcount = 0;

    void *old = NULL;
    if (!yacht_set(s->fd_hash_table, fd, info, &old)) {
        free(info);
        return false;
    }
    assert(old == NULL);
    return true;
}

static void increment_fd_refcount(sender *s, fd_info *fdi) {
    assert(fdi);
    if (fdi->refcount == 0) {
        /* Add to poll watch set */
        int idx = s->active_fds;
        assert(s->fds[idx + 1].fd == SENDER_FD_NOT_IN_USE);
        s->fds[idx + 1].fd = fdi->fd;
        assert(s->fds[idx + 1].events == POLLOUT);

        s->active_fds++;
    }

    fdi->refcount++;
}

static void decrement_fd_refcount(sender *s, fd_info *fdi) {
    assert(fdi);
    assert(fdi->refcount > 0);

    if (fdi->refcount == 1) {
        bool found = false;
        /* Remove from poll watch set */
        for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
            struct pollfd *pfd = &s->fds[CMD_FD + i];
            if (pfd->fd == fdi->fd) {
                if (s->active_fds > 1) {
                    /* Replace this one with last */
                    s->fds[CMD_FD + i].fd = s->fds[CMD_FD + s->active_fds - 1].fd;
                }
                s->fds[CMD_FD + s->active_fds - 1].fd = SENDER_FD_NOT_IN_USE;
                s->active_fds--;
                found = true;
                break;
            }
        }
        assert(found);
    }

    fdi->refcount--;
}

static bool release_socket_info(sender *s, int fd) {
    if (s->shutdown) { return false; }

    void *old = NULL;
    if (!yacht_remove(s->fd_hash_table, fd, &old)) {
        return false;
    }
    fd_info *info = (fd_info *)old;
    if (info) {
        assert(fd == info->fd);
        /* Expire any pending events on this socket. */
        if (info->refcount > 0) {
            set_error_for_socket(s, fd, TX_ERROR_CLOSED);
        }
        free(info);
        return true;
    } else {
        return false;
    }
}

static void handle_command(sender *s, int id) {
    assert(id < MAX_CONCURRENT_SENDS);
    tx_info_t *info = &s->tx_info[id];
    
    switch (info->state) {
    case TIS_ADD_SOCKET:
    {
        int fd = info->u.add_socket.fd;
        SSL *ssl = info->u.add_socket.ssl;
        if (register_socket_info(s, fd, ssl)) {
            notify_caller(s, info, true);
        } else {
            notify_caller(s, info, false);
        }
        break;
    }
    case TIS_RM_SOCKET:
    {
        int fd = info->u.rm_socket.fd;
        if (release_socket_info(s, fd)) {
            notify_caller(s, info, true);
        } else {
            notify_caller(s, info, false);
        }
        break;
    }
    case TIS_SHUTDOWN:
    {
        s->shutdown = true;
        /* Caller should be notified from cleanup(), by closing the
         * notification pipe. Block until shutdown is complete. */
        break;
    }
    case TIS_REQUEST_ENQUEUE:
    {
        enqueue_write(s, info);
        break;
    }

   /* Should not get anything else as an incoming command.  */
    default:
        assert(false);
    }
}

static void enqueue_write(struct sender *s, tx_info_t *info) {
    assert(info->state == TIS_REQUEST_ENQUEUE);

    int fd = info->u.enqueue.fd;
    fd_info *fdi = NULL;
    if (yacht_get(s->fd_hash_table, fd, (void **)&fdi)) {
        assert(fdi);
        /* Increment the refcount. This will cause poll to watch for the
         * socket being writable, if it isn't already being watched. */
        increment_fd_refcount(s, fdi);
        
        struct u_write uw = {
            .fd = info->u.enqueue.fd,
            .timeout_sec = TX_TIMEOUT,
            .box = info->u.enqueue.box,
            .fdi = fdi,
        };
        
        info->state = TIS_REQUEST_WRITE;
        info->u.write = uw;
        
    } else {
        set_error_for_socket(s, fd, TX_ERROR_UNREGISTERED_SOCKET);
    }
}

static bool check_incoming_commands(struct sender *s) {
    struct bus *b = s->bus;
    uint8_t buf[8];
    for (;;) {
        ssize_t rd = read(s->incoming_command_pipe, buf, sizeof(buf));
        
        if (rd == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            } else {
                BUS_LOG_SNPRINTF(b, 6 , LOG_SENDER, b->udata, 64,
                    "incoming_command poll: %s", strerror(errno));
                errno = 0;
                return false;
            }
        } else {
            for (int i = 0; i < rd; i++) {
                handle_command(s, buf[i]);
            }
            return true;
        }
    }
}

/* Get a tx_info write event associated with a socket. If there is one
 * that is already partially written, prefer that, to avoid interleaving
 * outgoing messages. */
static tx_info_t *get_info_to_write_for_socket(sender *s, int fd) {
    tx_info_t *res = NULL;
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = 1 << i;
        if (s->tx_flags & cur) {
            tx_info_t *info = &s->tx_info[i];
            if (info->state != TIS_REQUEST_WRITE) { continue; }
            if (info->u.write.fd == fd) {
                res = info;
                /* If we've already sent part of a message, send the rest
                 * before starting another, otherwise arbitrarily choose
                 * the one with the highest tx_info->id. */
                if (info->u.write.sent_size > 0) { return res; }
            }
        }
    }
    return res;
}

/* For every pending tx_info event associated with a given socket,
 * cancel it and notify its caller. */
static void set_error_for_socket(sender *s, int fd, tx_error_t error) {
    struct bus *b = s->bus;

    bus_send_status_t status = BUS_SEND_UNDEFINED;
    switch (error) {
    default:
        status = BUS_SEND_TX_FAILURE;
        break;
    case TX_ERROR_UNREGISTERED_SOCKET:
        status = BUS_SEND_UNREGISTERED_SOCKET;
        break;
    }

    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t cur = 1 << i;
        if (s->tx_flags & cur) {
            tx_info_t *info = &s->tx_info[i];

            bool notify = false;
            struct u_error ue = {
                .error = error,
            };

            switch (info->state) {

            /* TODO: There is a possible race here:
             *     Client thread writes TIS_REQUEST_ENQUEUE into a
             *     tx_info_t just as we're erroring it out. */
            case TIS_REQUEST_ENQUEUE:
                if (fd == info->u.enqueue.fd) {
                    ue.box = info->u.enqueue.box;
                    notify = true;
                }
                break;
            case TIS_REQUEST_WRITE:
                if (fd == info->u.write.fd) {
                    ue.box = info->u.write.box;
                    decrement_fd_refcount(s, info->u.write.fdi);
                    notify = true;
                }
                break;
            case TIS_RESPONSE_NOTIFY:
                if (fd == info->u.notify.fd) {
                    ue.box = info->u.notify.box;
                    notify = true;
                }
                break;
            default:
                break;
            }

            if (notify) {
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 64,
                    "setting error on socket %d", fd);
                info->state = TIS_ERROR;
                info->u.error = ue;
                notify_message_failure(s, info, status);
            }
        }
    }
}

static void attempt_write(sender *s, int available) {
    int written = 0;
    struct bus *b = s->bus;
    for (int i = 0; i < s->active_fds; i++) {
        if (written == available) { break; }  /* all done */
        
        struct pollfd *pfd = &s->fds[CMD_FD + i];
        assert(pfd->fd != SENDER_FD_NOT_IN_USE);
        BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
            "attempting write on %d (revents 0x%08x)", pfd->fd, pfd->revents);
        
        if (pfd->revents & POLLERR) {
            written++;
            set_error_for_socket(s, pfd->fd, TX_ERROR_POLLERR);
        } else if (pfd->revents & POLLHUP) {
            written++;
            set_error_for_socket(s, pfd->fd, TX_ERROR_POLLHUP);
        } else if (pfd->revents & POLLOUT) {
            written++;
            tx_info_t *info = get_info_to_write_for_socket(s, pfd->fd);
            if (info == NULL) {
                /* Polling is telling us a socket is writeable that we
                 * shouldn't care about. Should not get here. */
                printf(" *** NULL INFO ***\n");
                //assert(false);
                continue;
            }
            assert(info->state == TIS_REQUEST_WRITE);

            SSL *ssl = info->u.write.fdi->ssl;

            ssize_t wrsz = 0;
            if (ssl == BUS_NO_SSL) {
                wrsz = socket_write_plain(s, info);
            } else {
                assert(ssl);
                wrsz = socket_write_ssl(s, info, ssl);
            }
            BUS_LOG_SNPRINTF(b, 6, LOG_SENDER, b->udata, 64,
                "wrote %zd", wrsz);
        }
    }
}

static ssize_t socket_write_plain(sender *s, tx_info_t *info) {
    struct bus *b = s->bus;
    assert(info->state == TIS_REQUEST_WRITE);
    boxed_msg *box = info->u.write.box;
    uint8_t *msg = box->out_msg;
    size_t msg_size = box->out_msg_size;
    size_t sent_size = info->u.write.sent_size;
    size_t rem = msg_size - sent_size;
    
    int fd = info->u.write.fd;

    BUS_LOG_SNPRINTF(b, 10, LOG_SENDER, b->udata, 64,
        "write %p to %d, %zd bytes (info %d)",
        (void*)&msg[sent_size], fd, rem, info->id);
    ssize_t wrsz = write(fd, &msg[sent_size], rem);
    if (wrsz == -1) {
        if (util_is_resumable_io_error(errno)) {
            errno = 0;
            return 0;
        } else {
            /* close socket */
            BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                "write: socket error writing, %d", errno);
            set_error_for_socket(s, info->u.write.fd, TX_ERROR_WRITE_FAILURE);
            errno = 0;
            return 0;
        }
    } else if (wrsz > 0) {
        update_sent(b, s, info, wrsz);
        BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
            "sent: %zd\n", wrsz);
        return wrsz;
    } else {
        return 0;
    }
}

static ssize_t socket_write_ssl(sender *s, tx_info_t *info, SSL *ssl) {
    struct bus *b = s->bus;
    assert(info->state == TIS_REQUEST_WRITE);
    boxed_msg *box = info->u.write.box;
    uint8_t *msg = box->out_msg;
    size_t msg_size = box->out_msg_size;
    size_t rem = msg_size - info->u.write.sent_size;
    int fd = info->u.write.fd;
    ssize_t written = 0;

    while (rem > 0) {
        ssize_t wrsz = SSL_write(ssl, &msg[info->u.write.sent_size], rem);
        if (wrsz > 0) {
            update_sent(b, s, info, wrsz);
            written += wrsz;
            rem -= wrsz;
        } else if (wrsz < 0) {
            int reason = SSL_get_error(ssl, wrsz);
            switch (reason) {
            case SSL_ERROR_WANT_WRITE:
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: WANT_WRITE", fd);
                break;
                
            case SSL_ERROR_WANT_READ:
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
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
                } else {
                    set_error_for_socket(s, fd, TX_ERROR_WRITE_FAILURE);
                }
            }
            default:
            {
                BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 64,
                    "SSL_write: socket %d: error %d", info->u.write.fd, reason);
                break;
            }
            }
        } else {
            /* SSL_write should give SSL_ERROR_WANT_WRITE when unable
             * to write further. */
        }
    }
    return written;
}

static void update_sent(struct bus *b, sender *s, tx_info_t *info, ssize_t sent) {
    assert(info->state == TIS_REQUEST_WRITE);
    boxed_msg *box = info->u.write.box;
    size_t msg_size = box->out_msg_size;
    info->u.write.sent_size += sent;
    size_t rem = msg_size - info->u.write.sent_size;
    
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "wrote %zd, msg_size %zd (%p)",
        sent, msg_size, (void*)box->out_msg);
    if (rem == 0) { /* completed! */
        fd_info *fdi = info->u.write.fdi;

        struct u_notify un = {
            .fd = info->u.write.fd,
            .timeout_sec = info->u.write.timeout_sec,
            .box = info->u.write.box,
            .retries = info->u.write.retries,
        };

        /* Message has been sent, so release - caller may free it. */
        un.box->out_msg = NULL;

        info->state = TIS_RESPONSE_NOTIFY;
        info->u.notify = un;

        decrement_fd_refcount(s, fdi);

        BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
            "wrote all of %p, clearing", (void*)box->out_msg);
        attempt_to_enqueue_message_to_listener(s, info);
    }
}

static void notify_caller(sender *s, tx_info_t *info, bool success) {
    struct bus *b = s->bus;
    uint16_t bp = 0;

    switch (info->state) {
    case TIS_REQUEST_RELEASE:
        bp = info->u.release.backpressure;
        break;
    case TIS_ERROR:
        bp = info->u.error.backpressure;
        break;
    default:
        bp = 0;
        break;
    }

    uint8_t buf[sizeof(bool) + sizeof(bp)];
    buf[0] = success ? 0x01 : 0x00;
    buf[1] = (uint8_t)((bp & 0x00FF) >> 0);
    buf[2] = (uint8_t)((bp & 0xFF00) >> 8);
    
    int pipe_fd = s->pipes[info->id][1];
    
    ssize_t res = 0;

    for (;;) {                  /* for loop because of EINTR */
        res = write(pipe_fd, buf, sizeof(buf));
        BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
            "notify_caller: write %zd", res);

        if (res == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            } else {
                /* Note -- if this fails, a timeout upstream will take
                 *     care of it. But, under what circumstances can it
                 *     even fail? */
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDER, b->udata, 256,
                    "error on notify_caller write: %s", strerror(errno));
                errno = 0;
                break;
            }
        } else if (res == sizeof(buf)) {
            break;              /* success */
        } else {
            assert(false);
        }
    }
}

static void tick_handler(sender *s) {
    struct bus *b = s->bus;
    int tx_info_in_use = 0;
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t bit = (1 << i);
        if (s->tx_flags & bit) {
            tx_info_t *info = &s->tx_info[i];
            if (info->state != TIS_UNDEF) { tx_info_in_use++; }
        }
    }
    
    BUS_LOG_SNPRINTF(b, 2, LOG_SENDER, b->udata, 64,
        "tick... %p -- %d of %d tx_info in use, %d active FDs\n",
        (void*)s, tx_info_in_use, MAX_CONCURRENT_SENDS, s->active_fds);
    
    /* Walk table and expire timeouts & items which have been sent.
     *
     * All expiration of TX_INFO fields happens in here. */
    for (int i = 0; i < MAX_CONCURRENT_SENDS; i++) {
        tx_flag_t bit = (1 << i);
        if (s->tx_flags & bit) {  /* if info is in use */
            tx_info_t *info = &s->tx_info[i];
            switch (info->state) {
            case TIS_REQUEST_WRITE:
                tick_timeout(s, info);
                break;
            case TIS_RESPONSE_NOTIFY:
                tick_timeout(s, info);

                /* if not timed out, attempt to re-send */
                if (info->state == TIS_RESPONSE_NOTIFY) {
                    attempt_to_enqueue_message_to_listener(s, info);
                }
                break;

            default:
                break;
            }
        }
    }
}

static void tick_timeout(sender *s, tx_info_t *info) {
    switch (info->state) {
    case TIS_REQUEST_WRITE:
        if (info->u.write.timeout_sec == 1) { /* timed out */
            info->state = TIS_ERROR;
            struct u_error ue = {
                .error = TX_ERROR_WRITE_TIMEOUT,
                .box = info->u.write.box,
                .backpressure = 0,
            };
            info->u.error = ue;
            notify_message_failure(s, info, BUS_SEND_TX_TIMEOUT);
        } else {
            info->u.write.timeout_sec--;
        }
        break;
    case TIS_RESPONSE_NOTIFY:
        if (info->u.notify.timeout_sec == 1) { /* timed out */
            info->state = TIS_ERROR;
            struct u_error ue = {
                .error = TX_ERROR_NOTIFY_TIMEOUT,
                .box = info->u.write.box,
                .backpressure = 0,
            };
            info->u.error = ue;
            notify_message_failure(s, info, BUS_SEND_TX_TIMEOUT);
        } else {
            info->u.notify.timeout_sec--;
        }
        break;
    default:
        assert(false);
    }
}

static void attempt_to_enqueue_message_to_listener(sender *s, tx_info_t *info) {
    assert(info->state == TIS_RESPONSE_NOTIFY);
    struct bus *b = s->bus;
    /* Notify listener that it should expect a response to a
     * successfully sent message. */
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDER, b->udata, 128,
      "telling listener to expect response, with box %p", (void *)info->u.notify.box);
    
    struct listener *l = bus_get_listener_for_socket(s->bus, info->u.notify.fd);
    
    /* Release reference to the boxed message; if transferring ownership
     * to the listener succeeds, this will go out of scope. */
    struct boxed_msg *box = info->u.notify.box;
    info->u.notify.box = NULL;  /* passing on to listener */
    
    BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
        "deleting box %p for info->id %d (pass to listener)", (void *)box, info->id);

    uint16_t backpressure = 0;
    /* If this succeeds, then this thread cannot touch the box anymore. */
    if (listener_expect_response(l, box, &backpressure)) {
        info->state = TIS_REQUEST_RELEASE;
        info->u.release.backpressure = backpressure;

        notify_caller(s, info, true);
    } else {
        BUS_LOG(b, 2, LOG_SENDER, "failed delivery", b->udata);

        /* Put it back, since we need to keep managing it */
        info->u.notify.box = box;

        info->u.notify.retries++;
        if (info->u.notify.retries == SENDER_MAX_DELIVERY_RETRIES) {

            info->state = TIS_ERROR;
            struct u_error ue = {
                .error = TX_ERROR_NOTIFY_LISTENER_FAILURE,
                .box = box,
            };
            info->u.error = ue;

            notify_message_failure(s, info, BUS_SEND_RX_FAILURE);
            BUS_LOG(b, 2, LOG_SENDER, "failed delivery, several retries", b->udata);
        }
    }
}

static void notify_message_failure(sender *s, tx_info_t *info, bus_send_status_t status) {
    struct bus *b = s->bus;
    assert(info->state == TIS_ERROR);

    struct boxed_msg *box = info->u.error.box;
    box->result.status = status;
    
    size_t backpressure = 0;
    for (;;) {
        if (bus_process_boxed_message(s->bus, box, &backpressure)) {
            BUS_LOG_SNPRINTF(b, 5, LOG_SENDER, b->udata, 64,
                "deleting box %p for info->id %d (msg failure)",
                (void*)info->u.error.box, info->id);
            info->u.error.box = NULL;
            info->u.error.backpressure = backpressure;
            notify_caller(s, info, false);
            break;
        } else {
            poll(NULL, 0, 1);   /* sleep 1 msec and retry */
        }
    }
}
