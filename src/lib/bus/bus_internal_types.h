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

#ifndef BUS_INTERNAL_TYPES_H
#define BUS_INTERNAL_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "bus.h"
#include "yacht.h"

/* Struct for a message that will be passed from client to listener to
 * threadpool, proceeding directly to the threadpool if there is an error
 * along the way. This must only have a single owner at a time. */
typedef struct boxed_msg {

    /** Result message, constructed in place after the request/response cycle
     * has completed or failed due to timeout / unrecoverable error. */
    bus_msg_result_t result;

    /** Message send timeout. */
    time_t timeout_sec;

    /** Callback and userdata to which the bus_msg_result_t above will be sunk. */
    bus_msg_cb *cb;
    void *udata;

    /** Event timestamps to track timeouts. */
    struct timeval tv_send_start;
    struct timeval tv_send_done;

    /** Destination filename and message body. */
    int fd;
    SSL *ssl;                   ///< valid pointer or BUS_BOXED_MSG_NO_SSL
    int64_t out_seq_id;
    uint8_t *out_msg;
    size_t out_msg_size;
    size_t out_sent_size;
} boxed_msg;

/** Special "NO SSL" value, to distinguish from a NULL SSL handle. */
#define BUS_NO_SSL ((SSL *)-2)

typedef enum {
    SHUTDOWN_STATE_RUNNING = 0,
    SHUTDOWN_STATE_SHUTTING_DOWN,
    SHUTDOWN_STATE_HALTED,
} shutdown_state_t;

/** Message bus. */
typedef struct bus {
    bus_sink_cb *sink_cb;             ///< IO sink callback
    bus_unpack_cb *unpack_cb;         ///< Message unpacking callback
    bus_unexpected_msg_cb *unexpected_msg_cb; //< Unexpected message callback
    bus_error_cb *error_cb;           ///< Error handling callback
    void *udata;                      ///< User data for callbacks

    int log_level;                    ///< Log level
    bus_log_cb *log_cb;               ///< Logging callback

    uint8_t listener_count;           ///< Number of listeners
    struct listener **listeners;      ///< Listener array

    bool *joined;                     ///< Which threads have joined
    pthread_t *threads;               ///< Threads
    shutdown_state_t shutdown_state;  ///< Current shutdown state

    struct threadpool *threadpool;    ///< Thread pool
    SSL_CTX *ssl_ctx;                 ///< SSL context

    /** Locked hash table for fd -> connection_info */
    struct yacht *fd_set;
    pthread_mutex_t fd_set_lock;
} bus;

/** Special timeout value indicating UNBOUND. */
#define TIMEOUT_NOT_YET_SET ((time_t)(-1))

typedef enum {
    RX_ERROR_NONE = 0,
    RX_ERROR_READY_FOR_DELIVERY = 1,
    RX_ERROR_DONE = 2,
    RX_ERROR_POLLHUP = -31,
    RX_ERROR_POLLERR = -32,
    RX_ERROR_READ_FAILURE = -33,
    RX_ERROR_TIMEOUT = -34,
} rx_error_t;

/** Per-socket connection context. (Owned by the listener.) */
typedef struct {
    /* Shared */
    const int fd;
    const bus_socket_t type;
    void *udata;                ///< user connection data

    /* Shared, cleaned up by client */
    SSL *ssl;                   ///< SSL handle. Must be valid or BUS_NO_SSL.

    /** Set by client thread. Monotonically increasing max sequence ID. */
    int64_t largest_wr_seq_id_seen;

    /* Set by listener thread */
    rx_error_t error;
    size_t to_read_size;
} connection_info;

/** Arbitrary byte used to tag writes from the listener. */
#define LISTENER_MSG_TAG 0x15

/** Starting size^2 for file descriptor hash table. */
#define DEF_FD_SET_SIZE2 4

#endif
