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
#include <assert.h>
#include <string.h>
#include <err.h>
#include <signal.h>
#include <sys/time.h>
#include <poll.h>

#ifdef __Linux__
// some Linux distros put this in a nonstandard place.
#include <getopt.h>
#endif

#include "bus.h"
#include "atomic.h"
#include "socket99.h"
#include "util.h"

typedef struct {
    uint32_t magic_number;
    uint32_t size;
    int64_t seq_id;
} prot_header_t;

#define MAGIC_NUMBER 3

#define MAX_SOCKETS 1000
#define DEFAULT_BUF_SIZE (1024 * 1024 + sizeof(prot_header_t))
#define PRINT_RESPONSES 0

enum socket_state {
    STATE_UNINIT = 0,
    STATE_AWAITING_HEADER,
    STATE_AWAITING_BODY,
};

typedef struct {
    enum socket_state state;
    size_t cur_payload_size;
    size_t used;
    uint8_t buf[];
} socket_info;

typedef struct {
    int port_low;
    int port_high;
    int verbosity;
    size_t buf_size;

    int sockets_used;
    int sockets[MAX_SOCKETS];

    time_t last_second;
    size_t ticks;
    size_t sent_msgs;
    size_t completed_deliveries;
    size_t max_seq_id;

    socket_info *info[MAX_SOCKETS];
} example_state;

example_state state;

static void run_bus(example_state *s, struct bus *b);
static void parse_args(int argc, char **argv, example_state *s);
static time_t get_cur_second(void);

static void log_cb(log_event_t event, int log_level, const char *msg, void *udata) {
    example_state *s = (example_state *)udata;
    const char *event_str = Bus_LogEventStr(event);
    fprintf(/*stderr*/stdout, "%ld -- %s[%d] -- %s\n",
        s->last_second, event_str, log_level, msg);
}

#define LOG(VERBOSITY, ...)                     \
    do { if (state.verbosity >= VERBOSITY) { printf(__VA_ARGS__); } } while(0)

static const char *executable_name = NULL;

static bus_sink_cb_res_t reset_transfer(socket_info *si) {
    //printf("EXPECT: header of %zd bytes\n", sizeof(prot_header_t));
    bus_sink_cb_res_t res = { /* prime pump with header size */
        .next_read = sizeof(prot_header_t),
    };

    si->state = STATE_AWAITING_HEADER;
    si->used = 0;
    return res;
}

static bus_sink_cb_res_t sink_cb(uint8_t *read_buf,
        size_t read_size, void *socket_udata) {

    socket_info *si = (socket_info *)socket_udata;
    assert(si);

    switch (si->state) {
    case STATE_UNINIT:
    {
        assert(read_size == 0);
        return reset_transfer(si);
    }
    case STATE_AWAITING_HEADER:
    {
        bool valid_header = true;
        bool split_header = false;

        size_t header_rem = sizeof(prot_header_t) - si->used;
        if (read_size > header_rem) {
            printf("surplus read_size %zd\n", read_size);
            printf("header_rem %zd (sizeof(prot_header_t) %zd)\n", header_rem, sizeof(prot_header_t));
            assert(false);
        } else if (read_size < sizeof(prot_header_t)) {
            //printf("split header, %zd\n", read_size);
            split_header = true;
        }

        size_t copied = read_size;
        if (copied > header_rem) { copied = header_rem; }

        memcpy(&si->buf[si->used], read_buf, copied);
        si->used += copied;

        if (si->used < sizeof(prot_header_t)) {
            bus_sink_cb_res_t res = {
                .next_read = sizeof(prot_header_t) - si->used,
            };
            si->state = STATE_AWAITING_HEADER;
            return res;
        }

        assert(si->used == sizeof(prot_header_t));

        prot_header_t *header = (prot_header_t *)&si->buf[0];

        if (si->used < sizeof(prot_header_t)) {
            printf("INVALID HEADER A: read_size %zd\n", si->used);
            valid_header = false;
        } else if (header->magic_number != MAGIC_NUMBER) {
            printf("INVALID HEADER B: magic number 0x%08x\n", header->magic_number);
            valid_header = false;
        }

        if (valid_header) {
            prot_header_t *header = (prot_header_t *)&si->buf[0];
            si->cur_payload_size = header->size;
            memcpy(&si->buf[si->used], read_buf + copied, read_size - copied);
            si->used += read_size - copied;
            bus_sink_cb_res_t res = {
                .next_read = header->size,
            };
            si->state = STATE_AWAITING_BODY;
            return res;
        } else {
            assert(false);
            return reset_transfer(si);
        }

        break;
    }
    case STATE_AWAITING_BODY:
    {
        assert(DEFAULT_BUF_SIZE - si->used >= read_size);
        memcpy(&si->buf[si->used], read_buf, read_size);
        si->used += read_size;
        assert(si->used <= si->cur_payload_size + sizeof(prot_header_t));
        size_t rem = si->cur_payload_size + sizeof(prot_header_t) - si->used;

        if (rem == 0) {
            bus_sink_cb_res_t res = {
                .next_read = sizeof(prot_header_t),
                .full_msg_buffer = read_buf,
            };
            si->state = STATE_AWAITING_HEADER;
            si->used = 0;
            return res;
        } else {
            bus_sink_cb_res_t res = {
                .next_read = rem,
            };
            return res;
        }
    }
    default:
        assert(false);
    }
}

static bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata) {
    /* just got .full_msg_buffer from sink_cb -- pass it along as-is */
    socket_info *si = (socket_info *)socket_udata;
    prot_header_t *header = (prot_header_t *)&si->buf[0];
    uint8_t *payload = (uint8_t *)&si->buf[sizeof(prot_header_t)];

#if PRINT_RESPONSES
    for (int i = 0; i < si->used; i++) {
        if ((i & 15) == 0 && i > 0) { printf("\n"); }
        printf("%02x ", si->buf[i]);
    }
#endif

    bus_unpack_cb_res_t res = {
        .ok = true,
        .u.success = {
            .seq_id = header->seq_id,
            .msg = payload,
        },
    };
    return res;
}

static void unexpected_msg_cb(void *msg,
        int64_t seq_id, void *bus_udata, void *socket_udata) {
    printf("\n\n\nUNEXPECTED MESSAGE: %p, seq_id %lld, bus_udata %p, socket_udata %p\n\n\n\n",
        msg, (long long)seq_id, bus_udata, socket_udata);

    assert(false);
}

int main(int argc, char **argv) {
    executable_name = argv[0];
    state.last_second = get_cur_second();

    parse_args(argc, argv, &state);

    bus_config cfg = {
        .log_cb = log_cb,
        .log_level = state.verbosity,
        .sink_cb = sink_cb,
        .unpack_cb = unpack_cb,
        .unexpected_msg_cb = unexpected_msg_cb,
        .bus_udata = &state,
    };
    bus_result res = {0};
    if (!Bus_Init(&cfg, &res)) {
        LOG(0, "failed to init bus: %d\n", res.status);
        return 1;
    }

    struct bus *b = res.bus;

    run_bus(&state, b);

    if (b) {
        LOG(1, "shutting down\n");
        Bus_Shutdown(b);
        Bus_Free(b);
        return 0;
    } else {
        return 1;
    }
}

static bool loop_flag = true;
static sig_t old_sigint_handler = NULL;

static void signal_handler(int arg) {
    loop_flag = false;
    LOG(3, "-- caught signal %d\n", arg);
    if (arg == SIGINT) {
        signal(arg, old_sigint_handler);
    }
    (void)arg;
}

static sig_t register_signal_handler(int sig) {
    sig_t old_handler = signal(sig, signal_handler);
    if (old_handler == SIG_ERR) { err(1, "signal"); }
    return old_handler;
}

static void register_signal_handlers(void) {
    (void)register_signal_handler(SIGPIPE);
    old_sigint_handler = register_signal_handler(SIGINT);
}

static void usage(void) {
    fprintf(stderr,
        "Usage: %s [-b BUF_SIZE] [-l LOW_PORT] [-h HIGH_PORT] [-s STOP_AT_SEQUENCE_ID] [-v] \n"
        "    If only one of -l or -h are specified, it will use just that one port.\n"
        "    -v can be used multiple times to increase verbosity.\n"
        , executable_name);
    exit(1);
}

static void parse_args(int argc, char **argv, example_state *s) {
    int a = 0;

    s->buf_size = DEFAULT_BUF_SIZE;

    while ((a = getopt(argc, argv, "b:l:h:s:v")) != -1) {
        switch (a) {
        case 'b':               /* buffer size */
            s->buf_size = atol(optarg);
            break;
        case 'l':               /* low port */
            s->port_low = atoi(optarg);
            break;
        case 'h':               /* high port */
            s->port_high = atoi(optarg);
            break;
        case 's':               /* stop loop at sequence ID */
            s->max_seq_id = atoi(optarg);
            break;
        case 'v':               /* verbosity */
            s->verbosity++;
            break;
        default:
            fprintf(stderr, "illegal option: -- %c\n", a);
            usage();
        }
    }

    if (s->port_low == 0) { s->port_low = s->port_high; }
    if (s->port_high == 0) { s->port_high = s->port_low; }
    if (s->port_high < s->port_low || s->port_low == 0) { usage(); }

    if (s->verbosity > 0) {
        printf("bus_size: %zd\n", s->buf_size);
        printf("port_low: %d\n", s->port_low);
        printf("port_high: %d\n", s->port_high);
        printf("verbosity: %d\n", s->verbosity);
    }
}

static void open_sockets(example_state *s) {
    int socket_count = s->port_high - s->port_low + 1;

    size_t info_size = sizeof(socket_info) + s->buf_size;

    /* open socket(s) */
    for (int i = 0; i < socket_count; i++) {
        int port = i + s->port_low;

        socket99_config cfg = {
            .host = "127.0.0.1",
            .port = port,
            .nonblocking = true,
        };
        socket99_result res;

        if (!socket99_open(&cfg, &res)) {
            socket99_fprintf(stderr, &res);
            exit(1);
        }

        s->sockets[i] = res.fd;
        s->sockets_used++;
        socket_info *si = malloc(info_size);
        assert(si);
        memset(si, 0, info_size);
        s->info[i] = si;
    }
}

static size_t construct_msg(uint8_t *buf, size_t buf_size, size_t payload_size, int64_t seq_id) {
    size_t header_size = sizeof(prot_header_t);
    assert(buf_size > header_size);
    prot_header_t *header = (prot_header_t *)buf;

    uint8_t *payload = &buf[header_size];
    for (int i = 0; i < payload_size; i++) {
        payload[i] = (uint8_t)(0xFF & i);
    }

    header->magic_number = MAGIC_NUMBER;
    header->seq_id = seq_id;
    header->size = payload_size;

    return header_size + payload_size;
}

/* Should it CAS on the completion counter?
 * This should account for nearly all CPU usage. */
#define INCREMENT_COMPLETION_COUNTER 1

static void completion_cb(bus_msg_result_t *res, void *udata) {
    example_state *s = &state;
    socket_info *si = (socket_info *)udata;

    switch (res->status) {
    case BUS_SEND_SUCCESS:
    {
#if INCREMENT_COMPLETION_COUNTER
        for (;;) {
            size_t cur = s->completed_deliveries;
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->completed_deliveries, cur, cur + 1)) {
                LOG(3, " -- ! got %zd bytes, seq_id 0x%08llx, %p\n",
                    si->cur_payload_size, res->u.response.seq_id,
                    res->u.response.opaque_msg);
                break;
            }
        }
#else
        (void)s;
        (void)si;
#endif
    }
    break;
    case BUS_SEND_TX_TIMEOUT:
        LOG(1, "BUS_SEND_TIMEOUT\n");
        break;
    case BUS_SEND_TX_FAILURE:
        LOG(1, "BUS_SEND_TX_FAILURE\n");
        break;
    case BUS_SEND_RX_FAILURE:
        LOG(1, "BUS_SEND_RX_FAILURE\n");
        break;
    case BUS_SEND_RX_TIMEOUT:
        LOG(1, "BUS_SEND_RX_TIMEOUT\n");
        break;
    default:
        fprintf(stderr, "match fail: %d\n", res->status);
        assert(false);
    }
}

static void tick_handler(example_state *s) {
    s->ticks++;
    LOG(1, "%ld -- %zd ticks, %zd requests, %zd responses (delta %zd)\n",
        s->last_second, s->ticks, s->sent_msgs, s->completed_deliveries,
        s->completed_deliveries - s->sent_msgs);
}

static time_t get_cur_second(void) {
    struct timeval tv;
    if (!Util_Timestamp(&tv, true)) {
        assert(false);
    }
    return tv.tv_sec;
}

static void run_bus(example_state *s, struct bus *b) {
    register_signal_handlers();
    open_sockets(s);

    for (int i = 0; i < s->sockets_used; i++) {
        Bus_RegisterSocket(b, BUS_SOCKET_PLAIN, s->sockets[i], s->info[i]);
    }

    int cur_socket_i = 0;
    int64_t seq_id = 1;

    uint8_t msg_buf[DEFAULT_BUF_SIZE];
    size_t buf_size = sizeof(msg_buf);
    size_t payload_size = seq_id;

    s->last_second = get_cur_second();

    int sleep_counter = 0;
    int dropped = 0;

    while (loop_flag) {
        time_t cur_second = get_cur_second();
        if (cur_second != s->last_second) {
            tick_handler(s);
            s->last_second = cur_second;
            payload_size = 8;
            if (sleep_counter > 0) {
                sleep_counter--;
                if (sleep_counter == 0) {
                    printf(" -- resuming\n");
                }
            } else if ((cur_second & 0x3f) == 0x00) {
                printf(" -- sleeping for 10 seconds\n");
                sleep_counter = 10;
            }
        }

        if (sleep_counter == 0) {
            size_t msg_size = construct_msg(msg_buf, buf_size,
                100 * /*payload_size * */ 1024L, seq_id);
            LOG(3, " @@ sending message with %zd bytes\n", msg_size);
            bus_user_msg msg = {
                .fd = s->sockets[cur_socket_i],
                .seq_id = seq_id,
                .msg = msg_buf,
                .msg_size = msg_size,
                .cb = completion_cb,
                .udata = s->info[cur_socket_i],
            };

            s->sent_msgs++;
            payload_size++;
            if (!Bus_SendRequest(b, &msg)) {
                LOG(1, " @@@ Bus_SendRequest failed!\n");
                dropped++;
                if (dropped >= 100) {
                    LOG(1, " @@@ more than 100 send failures, halting\n");
                    loop_flag = false;
                }
            }

            cur_socket_i++;
            if (cur_socket_i == s->sockets_used) {
                cur_socket_i = 0;
            }
            seq_id++;
            if (seq_id == s->max_seq_id) {
                loop_flag = false;
            }
        } else {
            poll(NULL, 0, 1000);
        }
    }
}
