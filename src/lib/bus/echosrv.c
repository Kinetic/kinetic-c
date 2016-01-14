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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>

// For TCP_NODELAY
#include <netinet/tcp.h>

#include "socket99.h"
#include "util.h"

/* TODO: Linux => epoll */
#include <poll.h>

#define BUF_SZ (2 * 1024L * 1024)
#define MAX_CLIENTS 10

#define NO_CLIENT ((int)-1)

static uint8_t read_buf[BUF_SZ];

#define LOG(VERBOSITY, ...)                                            \
    do {                                                               \
        if (VERBOSITY <= cfg->verbosity) {                             \
            printf(__VA_ARGS__);                                       \
        }                                                              \
    }                                                                  \
    while(0)


typedef struct {
    int fd;
    size_t out_bytes;
    size_t written_bytes;
    uint8_t buf[2*BUF_SZ];
} out_buf;

typedef struct {
    int port_low;
    int port_high;
    int port_count;
    int verbosity;

    int ticks;
    time_t last_second;
    int successful_writes;
    int last_successful_writes;

    struct pollfd *accept_fds;
    struct pollfd *client_fds;

    int client_count;
    out_buf *out_bufs;
} config;

static void init_polling(config *cfg);
static void open_ports(config *cfg);
static void handle_incoming_connections(config *cfg, int available);
static void handle_client_io(config *cfg, int available);
static void listen_loop_poll(config *cfg);
static void register_client(config *cfg, int cfd,
    struct sockaddr *addr, socklen_t addr_len);
static void disconnect_client(config *cfg, int fd);
static void enqueue_write(config *cfg, int fd,
    uint8_t *buf, size_t write_size);

static void usage(void) {
    fprintf(stderr,
        "Usage: echosrv [-l LOW_PORT] [-h HIGH_PORT] [-v] \n"
        "    If only one of -l or -h are specified, it will use just that one port.\n"
        "    -v can be used multiple times to increase verbosity.\n");
    exit(1);
}

static void parse_args(int argc, char **argv, config *cfg) {
    int a = 0;

    while ((a = getopt(argc, argv, "l:h:v")) != -1) {
        switch (a) {
        case 'l':               /* low port */
            cfg->port_low = atoi(optarg);
            break;
        case 'h':               /* high port */
            cfg->port_high = atoi(optarg);
            break;
        case 'v':               /* verbosity */
            cfg->verbosity++;
            break;
        default:
            fprintf(stderr, "illegal option: -- %c\n", a);
            usage();
        }
    }

    if (cfg->port_low == 0) { cfg->port_low = cfg->port_high; }
    if (cfg->port_high == 0) { cfg->port_high = cfg->port_low; }
    if (cfg->port_high < cfg->port_low || cfg->port_low == 0) { usage(); }
    if (cfg->verbosity > 0) { printf("verbosity: %d\n", cfg->verbosity); }
}

int main(int argc, char **argv) {
    config cfg;
    memset(&cfg, 0, sizeof(cfg));
    parse_args(argc, argv, &cfg);

    init_polling(&cfg);
    open_ports(&cfg);
    listen_loop_poll(&cfg);

    return 0;
}

static void init_polling(config *cfg) {
    cfg->port_count = cfg->port_high - cfg->port_low + 1;

    size_t accept_fds_sz = cfg->port_count * sizeof(struct pollfd);
    struct pollfd *accept_fds = malloc(accept_fds_sz);
    assert(accept_fds);
    memset(accept_fds, 0, accept_fds_sz);
    cfg->accept_fds = accept_fds;

    size_t client_fds_sz = MAX_CLIENTS * sizeof(struct pollfd);
    struct pollfd *client_fds = malloc(client_fds_sz);
    assert(client_fds);
    memset(client_fds, 0, client_fds_sz);
    cfg->client_fds = client_fds;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        cfg->client_fds[i].fd = NO_CLIENT;
    }

    size_t out_bufs_sz = MAX_CLIENTS * sizeof(out_buf);
    cfg->out_bufs = malloc(out_bufs_sz);
    assert(cfg->out_bufs);
    memset(cfg->out_bufs, 0, out_bufs_sz);
}

static void open_ports(config *cfg) {
    socket99_config scfg = {
        .host = "127.0.0.1",
        .port = 0,
        .server = true,
        .nonblocking = true,
    };

    socket99_result res;

    for (int i = 0; i < cfg->port_count; i++) {
        scfg.port = i + cfg->port_low;
        bool ok = socket99_open(&scfg, &res);
        if (!ok) {
            fprintf(stderr, "Error opening port %d: ", i + cfg->port_low);
            socket99_fprintf(stderr, &res);
            exit(1);
        } else {
            cfg->accept_fds[i].fd = res.fd;
            cfg->accept_fds[i].events = (POLLIN);
            LOG(2, " -- Accepting on %s:%d\n", scfg.host, scfg.port);
        }
    }
}

#define MAX_TIMEOUT 1000

static void tick_handler(config *cfg) {
    cfg->ticks++;
    LOG(1, "%ld -- client_count: %d, successful writes: %d (avg %d/sec, delta %d)\n",
        cfg->last_second, cfg->client_count, cfg->successful_writes,
        cfg->successful_writes / cfg->ticks,
        cfg->successful_writes - cfg->last_successful_writes);
    cfg->last_successful_writes = cfg->successful_writes;
}

static void listen_loop_poll(config *cfg) {
    struct timeval tv;

    if (!Util_Timestamp(&tv, true)) { assert(false); }
    cfg->last_second = tv.tv_sec;

    assert(cfg->client_fds[0].fd == NO_CLIENT);

    int delay = 1;

    for (;;) {
        if (!Util_Timestamp(&tv, true)) { assert(false); }
        if (tv.tv_sec != cfg->last_second) {
            tick_handler(cfg);
            cfg->last_second = tv.tv_sec;
        }

        int accept_delay = 0;
        int client_delay = 0;

        if (cfg->client_count > 0) {
            client_delay = delay;
        } else if (cfg->client_count < MAX_CLIENTS) {
            accept_delay = delay;
        } else {
            assert(false);
        }

        if (cfg->client_count < MAX_CLIENTS) {
            /* Listen for incoming connections */
            int res = poll(cfg->accept_fds, cfg->port_count, accept_delay);
            LOG((res == 0 ? 6 : 3), "accept poll res %d\n", res);

            if (res == -1) {
                if (Util_IsResumableIOError(errno)) {
                    errno = 0;
                } else {
                    err(1, "poll");
                }
            } else if (res == 0) {
                /* nothing */
            } else {
                handle_incoming_connections(cfg, res);
                delay = 0;
            }
        }

        if (cfg->client_count > 0) {
            int res = poll(cfg->client_fds, cfg->client_count, client_delay);
            delay <<= 1;
            LOG((res == 0 ? 6 : 3), "client poll res %d\n", res);
            /* Read / write to clients */
            if (res == -1) {
                if (Util_IsResumableIOError(errno)) {
                    errno = 0;
                } else {
                    err(1, "poll");
                }
            } else if (res == 0) {
                /* nothing */
            } else {
                LOG(3, "poll(client_fds, %d) => res of %d\n",
                    cfg->client_count, res);
                handle_client_io(cfg, res);
                delay = 0;
            }
        }

        if (delay == 0) {
            delay = 1;
        } else {
            delay *= 2;
            if (delay > MAX_TIMEOUT) { delay = MAX_TIMEOUT; }
        }
    }
}

static void handle_incoming_connections(config *cfg, int available) {
    int checked = 0;
    for (int i = 0; i < cfg->port_count; i++) {
        if (checked == available) { break; }
        struct pollfd *fd = &cfg->accept_fds[i];
        if ((fd->revents & POLLERR) || (fd->revents & POLLHUP)) {
            assert(false);
        } else if (fd->revents & POLLIN) {
            checked++;
            struct sockaddr address;
            socklen_t addr_len;
            int client_fd = accept(fd->fd, &address, &addr_len);
            if (client_fd == -1) {
                if (errno == EWOULDBLOCK) {
                    errno = 0;
                    continue;
                } else {
                    close(fd->fd);
                    err(1, "listen");
                    //continue;
                }
            }

            LOG(2, "accepting client %d\n", client_fd);
            register_client(cfg, client_fd, &address, addr_len);
        }
    }
}

static void register_client(config *cfg, int cfd,
        struct sockaddr *addr, socklen_t addr_len) {

    /* assign to first empty slot */
    int client_index = 0;
    for (client_index = 0; client_index < MAX_CLIENTS; client_index++) {
        LOG(4, " -- cfg->client_fds[%d].fd == %d\n", client_index, cfg->client_fds[client_index].fd);
        if (cfg->client_fds[client_index].fd == NO_CLIENT) { break; }
    }
    assert(client_index != MAX_CLIENTS);
    LOG(3, " -- assigning client in slot %d\n", client_index);

    out_buf *out = &cfg->out_bufs[client_index];
    out->fd = cfd;
    out->out_bytes = 0;

    int flag = 1;
    if (0 != setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int))) {
        err(1, "setsockopt");
    }

    struct pollfd *fd = &cfg->client_fds[client_index];
    fd->fd = cfd;
    fd->events = (POLLIN);

    cfg->client_count++;
}

static void handle_client_io(config *cfg, int available) {
    int checked = 0;
    for (int i = 0; i < cfg->client_count; i++) {
        if (checked == available) { break; }
        struct pollfd *fd = &cfg->client_fds[i];

        LOG(4, "fd[%d]->events 0x%08x ==> revents: 0x%08x\n", i, fd->events, fd->revents);

        if ((fd->revents & POLLERR) || (fd->revents & POLLHUP)) {
            LOG(3, "Disconnecting client %d\n", fd->fd);
            disconnect_client(cfg, fd->fd);
        } else if (fd->revents & POLLOUT) {
            checked++;
            out_buf *buf = &cfg->out_bufs[i];
            LOG(2, "writing %zd bytes to %d\n", buf->out_bytes, buf->fd);
            size_t wr_size = buf->out_bytes - buf->written_bytes;
            ssize_t wres = write(buf->fd, &buf->buf[buf->written_bytes], wr_size);
            if (wres == -1) {
                if (Util_IsResumableIOError(errno)) {
                    errno = 0;
                } else if (errno == EPIPE) {
                    disconnect_client(cfg, fd->fd);
                } else {
                    err(1, "write");
                }
            } else {
                buf->written_bytes += wres;
                if (buf->written_bytes == buf->out_bytes) {
                    buf->out_bytes = 0;
                    buf->written_bytes = 0;
                    cfg->successful_writes++;
                    fd->events = POLLIN;
                }
            }
        } else if (fd->revents & POLLIN) {
            checked++;
            /* if we can read, then queue up same as a write */
            out_buf *buf = &cfg->out_bufs[i];
            ssize_t rres = read(buf->fd, read_buf, BUF_SZ - 1);

            if (rres == -1) {
                if (Util_IsResumableIOError(errno)) {
                    errno = 0;
                } else if (errno == EPIPE) {
                    disconnect_client(cfg, fd->fd);
                } else {
                    err(1, "read");
                }
            } else if (rres > 0) {
                /* enqueue outgoing write */
                LOG(2, "%ld -- got %zd bytes\n",
                    cfg->last_second, rres);
                enqueue_write(cfg, buf->fd, read_buf, rres);
            } else {
                LOG(2, "else, rres %zd\n", rres);
            }
        }
    }
}

static void enqueue_write(config *cfg, int fd,
        uint8_t *buf, size_t write_size) {
    assert(write_size <= BUF_SZ);

    for (int i = 0; i < cfg->client_count; i++) {
        out_buf *out = &cfg->out_bufs[i];
        if (fd == out->fd) {
            buf[write_size] = '\0';
            LOG(2, "%ld -- enqueing write of %zd bytes\n",
                cfg->last_second, write_size);

            size_t free_space = BUF_SZ - out->out_bytes;
            assert(free_space >= write_size);
            memcpy(&out->buf[out->out_bytes], buf, write_size);
            out->out_bytes += write_size;

            cfg->client_fds[i].events = POLLOUT;   /* write only */
            return;
        }
    }

    assert(false);
}

static void disconnect_client(config *cfg, int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (cfg->client_fds[i].fd == fd) {
            LOG(3, "disconnecting client %d\n", fd);
            cfg->client_fds[i].fd = NO_CLIENT;
            close(fd);
            cfg->out_bufs[i].fd = NO_CLIENT;
            cfg->out_bufs[i].out_bytes = 0;
            cfg->client_count--;
            return;
        }
    }

    assert(false);              /* not found */
}
