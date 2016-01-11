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

/* Default number of seconds before a message response times out. */
#define BUS_DEFAULT_TIMEOUT_SEC 10

/* Special sequence ID value indicating none was available. */
#define BUS_NO_SEQ_ID (-1)

#ifdef TEST
#define BUS_LOG(B, LEVEL, EVENT_KEY, MSG, UDATA) (void)B
#define BUS_LOG_SNPRINTF(B, LEVEL, EVENT_KEY, UDATA, MAX_SZ, FMT, ...) (void)B
#else
#define BUS_LOG(B, LEVEL, EVENT_KEY, MSG, UDATA)                       \
    do {                                                               \
        struct bus *_b = (B);                                          \
        int _level = LEVEL;                                            \
        log_event_t _event_key = EVENT_KEY;                            \
        char *_msg = MSG;                                              \
        void *_udata = UDATA;                                          \
        if (_b->log_level >= _level && _b->log_cb != NULL) {           \
            _b->log_cb(_event_key, _level, _msg, _udata);              \
        }                                                              \
    } while (0)

#define BUS_LOG_STRINGIFY(X) #X

#define BUS_LOG_SNPRINTF(B, LEVEL, EVENT_KEY, UDATA, MAX_SZ, FMT, ...) \
    do {                                                               \
        struct bus *_b = (B);                                          \
        int _level = LEVEL;                                            \
        log_event_t _event_key = EVENT_KEY;                            \
        void *_udata = UDATA;                                          \
        if (_b->log_level >= _level && _b->log_cb != NULL) {           \
            char _log_buf[MAX_SZ];                                     \
            if (MAX_SZ < snprintf(_log_buf, MAX_SZ,                    \
                    FMT, __VA_ARGS__)) {                               \
                _b->log_cb(_event_key, _level,                         \
                    "snprintf failure -- "                             \
                    __FILE__,                                          \
                    _udata);                                           \
                char _line_buf[32];                                    \
                snprintf(_line_buf, 32, "line %d\n", __LINE__);        \
                _b->log_cb(_event_key, _level, _line_buf, _udata);     \
            } else {                                                   \
                _b->log_cb(_event_key, _level, _log_buf, _udata);      \
            }                                                          \
        }                                                              \
    } while (0)
#endif

#define BUS_ASSERT(B, UDATA, COND) \
    do { \
        if(!(COND)) \
        { \
            BUS_LOG_SNPRINTF(B, 0, LOG_ASSERT, UDATA, 128, \
                "BUS FAILURE at %s:%d in %s: assert(" #COND ")", \
                __FILE__, (int)__LINE__, __FUNCTION__); \
            assert(COND); \
        } \
    } while(0)

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
    LOG_ASSERT,
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
 * The (void *) that was passed in during Bus_RegisterSocket will be
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

/* Handle a result from bus_unpack_cb that is marked as an error. */
typedef void (bus_error_cb)(bus_unpack_cb_res_t result, void *socket_udata);

/* Process a message that was successfully unpacked, but does not have
 * an expected sequence ID. This is likely a keepalive message, status
 * message, etc. */
typedef void (bus_unexpected_msg_cb)(void *msg,
    int64_t seq_id, void *bus_udata, void *socket_udata);

/* Configuration for the messaging bus */
typedef struct bus_config {
    /* If omitted, these fields will be set to defaults. */
    int listener_count;
    struct threadpool_config threadpool_cfg;

    /* Callbacks */
    bus_sink_cb *sink_cb;       /* required */
    bus_unpack_cb *unpack_cb;   /* required */
    bus_unexpected_msg_cb *unexpected_msg_cb;
    bus_error_cb *error_cb;

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
} Bus_Init_res_t;

typedef enum {
    BUS_SEND_UNDEFINED = 0,
    BUS_SEND_REQUEST_COMPLETE = 1,
    BUS_SEND_SUCCESS = 3,
    BUS_SEND_TX_TIMEOUT = -51,
    BUS_SEND_TX_FAILURE = -52,  // -> socket error
    BUS_SEND_RX_TIMEOUT = -53,
    BUS_SEND_RX_FAILURE = -54,
    BUS_SEND_BAD_RESPONSE = -55,
    BUS_SEND_UNREGISTERED_SOCKET = -56,
    BUS_SEND_RX_TIMEOUT_EXPECT = -57,
    BUS_SEND_TX_TIMEOUT_NOTIFYING_LISTENER = -58,
    BUS_SEND_TIMESTAMP_ERROR = -59,
} bus_send_status_t;

/* Result from attempting to configure a message bus. */
typedef struct bus_result {
    Bus_Init_res_t status;
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
            void *opaque_msg;
        } response;
    } u;
} bus_msg_result_t;

typedef void (bus_msg_cb)(bus_msg_result_t *res, void *udata);

typedef enum {
    BUS_SOCKET_PLAIN,
    BUS_SOCKET_SSL,
} bus_socket_t;

/* A message being packaged for delivery by the message bus. */
typedef struct {
    int fd;
    bus_socket_t type;
    int64_t seq_id;
    uint8_t *msg;
    size_t msg_size;
    uint16_t timeout_sec;
    bool no_response;
    bus_msg_cb *cb;
    void *udata;
} bus_user_msg;

/* This opaque bus struct represents the only user-facing interface to
 * the network handling code. Callbacks are provided to react to network
 * events. */
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
