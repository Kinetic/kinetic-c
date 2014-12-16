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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <signal.h>
#include <sys/time.h>

#ifdef __Linux__
// some Linux distros put this in a nonstandard place.
#include <getopt.h>
#endif

#include "bus.h"
#include "atomic.h"
#include "socket99.h"

#define MAX_SOCKETS 1000
#define DEFAULT_BUF_SIZE (32 * 1024)
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
    const char *event_str = bus_log_event_str(event);
    fprintf(stderr, "%ld -- %s[%d] -- %s\n",
        s->last_second, event_str, log_level, msg);
}

#define LOG(VERBOSITY, ...)                     \
    do { if (state.verbosity >= VERBOSITY) { printf(__VA_ARGS__); } } while(0)

typedef struct {
    uint32_t magic_number;
    uint32_t size;
    int64_t seq_id;
} prot_header_t;

#define MAGIC_NUMBER 3

static const char *executable_name = NULL;

static bus_sink_cb_res_t reset_transfer(socket_info *si) {
    //printf("EXPECT: header of %zd bytes\n", sizeof(prot_header_t));
    bus_sink_cb_res_t res = { /* prime pump with header size */
        .next_read = sizeof(prot_header_t),
    };
    
    si->state = STATE_AWAITING_HEADER;
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

        prot_header_t *header = (prot_header_t *)read_buf;
        if (read_size != sizeof(prot_header_t)) {
            valid_header = false;
        } else if (header->magic_number != MAGIC_NUMBER) {
            valid_header = false;
        }

        if (valid_header) {
            uint8_t *buf = si->buf;
            prot_header_t *header = (prot_header_t *)read_buf;
            si->cur_payload_size = header->size;
            memcpy(buf, read_buf, sizeof(prot_header_t));
            si->used = sizeof(*header);

            bus_sink_cb_res_t res = {
                .next_read = header->size,
            };
            si->state = STATE_AWAITING_BODY;
            return res;
        } else {
            return reset_transfer(si);
        }
        
        break;
    }
    case STATE_AWAITING_BODY:
    {
        if (read_size == si->cur_payload_size) {
            bus_sink_cb_res_t res = {
                .next_read = sizeof(prot_header_t),
                .full_msg_buffer = read_buf,
            };
            memcpy(&si->buf[si->used], read_buf, read_size);
            si->used += read_size;
            si->state = STATE_AWAITING_HEADER;
            return res;
        } else {
            return reset_transfer(si);
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
        msg, seq_id, bus_udata, socket_udata);
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
    if (!bus_init(&cfg, &res)) {
        LOG(0, "failed to init bus: %d\n", res.status);
        return 1;
    }

    struct bus *b = res.bus;

    run_bus(&state, b);

    if (b) {
        LOG(1, "shutting down\n");
        bus_shutdown(b);
        bus_free(b);
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
        size_t cur = s->completed_deliveries;
        for (;;) {
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&s->completed_deliveries, cur, cur + 1)) {
                LOG(4, " -- ! got %zd bytes, seq_id 0x%08llx, %p\n",
                    si->cur_payload_size, res->u.response.seq_id,
                    res->u.response.opaque_msg);
                break;
            }
        }
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
    gettimeofday(&tv, 0);
    return tv.tv_sec;
}

static void run_bus(example_state *s, struct bus *b) {
    register_signal_handlers();
    open_sockets(s);

    for (int i = 0; i < s->sockets_used; i++) {
        bus_register_socket(b, s->sockets[i], s->info[i]);
    }

    bool should_send = true;
    int cur_socket_i = 0;
    int64_t seq_id = 1;

    uint8_t msg_buf[DEFAULT_BUF_SIZE];
    size_t buf_size = sizeof(msg_buf);
    size_t payload_size = seq_id;

    s->last_second = get_cur_second();

    while (loop_flag) {
        time_t cur_second = get_cur_second();
        if (cur_second != s->last_second) {
            tick_handler(s);
            s->last_second = cur_second;
            payload_size = 8;
        } else {
            should_send = true;
        }

        if (should_send) {
            should_send = false;
            size_t msg_size = construct_msg(msg_buf, buf_size, 10*payload_size, seq_id);
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
            if (!bus_send_request(b, &msg)) {
                LOG(1, " @@@ bus_send_request failed!\n");
            }
            
            cur_socket_i++;
            if (cur_socket_i == s->sockets_used) {
                cur_socket_i = 0;
            }
            seq_id++;
            if (seq_id == s->max_seq_id) {
                loop_flag = false;
            }
        }
    }
}
