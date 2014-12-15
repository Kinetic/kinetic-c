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
#ifndef BUS_TYPES_H
#define BUS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "threadpool.h"

/* Boxed type for the internal state used while asynchronously
 * processing an message. */
struct boxed_msg;

/* Max number of concurrent sends that can be active. */
#define BUS_MAX_CONCURRENT_SENDS 10

#define BUS_LOG(B, LEVEL, EVENT_KEY, MSG, UDATA)                       \
    do {                                                               \
        struct bus *_b = (B);                                          \
        int level = LEVEL;                                             \
        log_event_t event_key = EVENT_KEY;                             \
        char *msg = MSG;                                               \
        void *udata = UDATA;                                           \
        if (_b->log_level >= level) {                                  \
            bus_lock_log(_b);                                          \
            _b->log_cb(event_key, level, msg, udata);                  \
            bus_unlock_log(_b);                                        \
        }                                                              \
    } while (0)                                                        \

#define BUS_LOG_STRINGIFY(X) #X

#define BUS_LOG_SNPRINTF(B, LEVEL, EVENT_KEY, UDATA, MAX_SZ, FMT, ...) \
    do {                                                               \
        struct bus *_b = (B);                                          \
        int level = LEVEL;                                             \
        log_event_t event_key = EVENT_KEY;                             \
        void *udata = UDATA;                                           \
        if (_b->log_level >= level) {                                  \
            bus_lock_log(_b);                                          \
            char log_buf[MAX_SZ];                                      \
            if (MAX_SZ < snprintf(log_buf, MAX_SZ,                     \
                    FMT, __VA_ARGS__)) {                               \
                _b->log_cb(event_key, level,                           \
                    "snprintf failure -- "                             \
                    __FILE__,                                          \
                    udata);                                            \
            } else {                                                   \
                _b->log_cb(event_key, level, log_buf, udata);          \
            }                                                          \
            bus_unlock_log(_b);                                        \
        }                                                              \
    } while (0)                                                        \

/* Event tag for a log message. */
typedef enum {
    LOG_INITIALIZATION,
    LOG_NEW_CLIENT,
    LOG_SOCKET_REGISTERED,
    LOG_SENDING_REQUEST,
    LOG_SHUTDOWN,
    LOG_SENDER,
    LOG_LISTENER,
    LOG_MEMORY,
    /* ... */
    LOG_EVENT_TYPE_COUNT,
} log_event_t;

/* Pass an event to log. */
typedef void (bus_log_cb)(log_event_t event, int log_level, const char *msg, void *udata);

/* Result from bus_sink_cb. See below. */
typedef struct {
    size_t next_read;           /* size for next read */
    void *full_msg_buffer;      /* can be NULL */
} bus_sink_cb_res_t;

/* Sink READ_SIZE bytes in READ_BUF into a protocol handler. This read
 * size is based on the previously requested size. (If the size for the
 * next request is undefined, this can be called with a READ_SIZE of 0.)
 *
 * The (void *) that was passed in during bus_register_socket will be
 * passed along.
 *
 * A bus_sink_cb_res_t struct should be returned, with next_read
 * indicating that the callback should be called again once NEXT_READ
 * bytes are available (more may be buffered internally). If
 * FULL_MSG_BUFFER is non-NULL, then that buffer will be passed to
 * BUS_UNPACK_CB (below) for further processing. */
typedef bus_sink_cb_res_t (bus_sink_cb)(uint8_t *read_buf,
    size_t read_size, void *socket_udata);

/* Result from an attempting to unpack a message according to the user
 * protocol's unpack callback. See bus_unpack_cb below. */
typedef struct {
    bool ok;                    /* could be a type tag */
    union {
        struct {
            void *msg;          /* protocol message to sink to CB */
            int64_t seq_id;     /* sequence ID for message */
        } success;
        struct {
            uintptr_t opaque_error_id;
        } error;
    } u;    
} bus_unpack_cb_res_t;

/* Unpack a message that has been gradually accumulated via bus_sink_cb.
 * 
 * Note that the udata pointer is socket-specific, NOT client-specific. */
typedef bus_unpack_cb_res_t (bus_unpack_cb)(void *msg, void *socket_udata);

/* Process a message that was successfully unpacked, but does not have
 * an expected sequence ID. This is likely a keepalive message, status
 * message, etc. */
typedef void (bus_unexpected_msg_cb)(void *msg,
    int64_t seq_id, void *bus_udata, void *socket_udata);

/* Configuration for the messaging bus */
typedef struct bus_config {
    /* If omitted, these fields will be set to defaults. */
    int sender_count;
    int listener_count;
    struct threadpool_config threadpool_cfg;

    /* Callbacks */
    bus_sink_cb *sink_cb;       /* required */
    bus_unpack_cb *unpack_cb;   /* required */
    bus_unexpected_msg_cb *unexpected_msg_cb;

    int log_level;
    bus_log_cb *log_cb;         /* optional */
    
    void *bus_udata;
} bus_config;

typedef enum {
    BUS_INIT_SUCCESS = 0,
    BUS_INIT_ERROR_NULL = -1,
    BUS_INIT_ERROR_MISSING_SINK_CB = -2,
    BUS_INIT_ERROR_MISSING_UNPACK_CB = -3,
    BUS_INIT_ERROR_ALLOC_FAIL = -4,
    BUS_INIT_ERROR_SENDER_INIT_FAIL = -5,
    BUS_INIT_ERROR_LISTENER_INIT_FAIL = -6,
    BUS_INIT_ERROR_THREADPOOL_INIT_FAIL = -7,
    BUS_INIT_ERROR_PTHREAD_INIT_FAIL = -8,
    BUS_INIT_ERROR_MUTEX_INIT_FAIL = -9,
} bus_init_res_t;

typedef enum {
    BUS_SEND_UNDEFINED = 0,
    BUS_SEND_SUCCESS = 1,
    BUS_SEND_TX_TIMEOUT = -1,
    BUS_SEND_TX_FAILURE = -2,
    BUS_SEND_RX_TIMEOUT = -3,
    BUS_SEND_RX_FAILURE = -4,
    BUS_SEND_BAD_RESPONSE = -5,
} bus_send_status_t;

/* Result from attempting to configure a message bus. */
typedef struct bus_result {
    bus_init_res_t status;
    struct bus *bus;
} bus_result;

/* Callback for a request's result. */
typedef struct {
    bus_send_status_t status; // request_status
    union {
        struct {
            const char *err_str;
        } error_minutiae;
        struct {
            // user needs to free *msg
            int64_t seq_id;
            uint8_t *opaque_msg;
        } response;
    } u;
} bus_msg_result_t;

typedef void (bus_msg_cb)(bus_msg_result_t *res, void *udata);

typedef struct {
    int fd;
    uint64_t seq_id;
    uint8_t *msg;
    size_t msg_size;

    bus_msg_cb *cb;
    void *udata;
} bus_user_msg;

struct bus_t;

typedef enum {
    /* Message successfully processed. */
    BUS_REQUEST_SUCCESS,

    /* Tried te send a request, timed out */
    BUS_REQUEST_SEND_FAILURE_TIMEOUT,

    /* Tried to send a request, remote end hung up */
    BUS_REQUEST_SEND_FAILURE_HUP,

    /* Timeout while waiting for response */
    BUS_RESPONSE_FAILURE_TIMEOUT,

    /* Remote end hung up while waiting for response */
    BUS_RESPONSE_FAILURE_HUP,
} bus_status_res_t;


#endif
