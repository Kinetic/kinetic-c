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
#ifndef LISTENER_INTERNAL_TYPES_H
#define LISTENER_INTERNAL_TYPES_H

#include "bus_types.h"
#include "bus_internal_types.h"

#include <poll.h>

/** Default size for the read buffer, which will grow on demand. */
#define DEFAULT_READ_BUF_SIZE (1024L * 1024L)

/** ID of the `struct pollfd` for the listener's incoming command
 * pipe. This is in the same pollfd array as the sockets being
 * watched so that an incoming command will wake it from its
 * blocking poll. */
#define INCOMING_MSG_PIPE_ID 0

/** Offset to account for the first file descriptor being the incoming
 * message pipe. */
#define INCOMING_MSG_PIPE 1

typedef enum {
    MSG_NONE,
    MSG_ADD_SOCKET,
    MSG_REMOVE_SOCKET,
    MSG_HOLD_RESPONSE,
    MSG_EXPECT_RESPONSE,
    MSG_SHUTDOWN,
} MSG_TYPE;

/** A queue message, with a command in the tagged union. */
typedef struct listener_msg {
    const uint8_t id;
    MSG_TYPE type;
    struct listener_msg *next;
    int pipes[2];

    union {                     /* keyed by .type */
        struct {
            connection_info *info;
            int notify_fd;
        } add_socket;
        struct {
            int fd;
            int notify_fd;
        } remove_socket;
        struct {
            int fd;
            int64_t seq_id;
            int16_t timeout_sec;
            int notify_fd;
        } hold;
        struct {
            boxed_msg *box;
        } expect;
        struct {
            int notify_fd;
        } shutdown;
    } u;
} listener_msg;

/** How long the listener should wait for responses before becoming idle
 * and blocking. */
#define LISTENER_TASK_TIMEOUT_DELAY 100

typedef enum {
    RIS_HOLD = 1,
    RIS_EXPECT = 2,
    RIS_INACTIVE = 3,
} rx_info_state;

/** Record in table for partially processed messages. */
typedef struct rx_info_t {
    const uint16_t id;
    struct rx_info_t *next;

    rx_info_state state;
    time_t timeout_sec;

    union {
        struct {
            int fd;
            int64_t seq_id;
            bool has_result;
            bus_unpack_cb_res_t result;
            rx_error_t error;
        } hold;
        struct {
            boxed_msg *box;
            rx_error_t error;
            bool has_result;
            bus_unpack_cb_res_t result;
        } expect;
    } u;
} rx_info_t;

/** Max number of sockets to monitor.
 * If listening to more sockets than this, use multiple listener threads. */
#define MAX_FDS 1000

/* Max number of partially processed messages.
 * TODO: Capacity planning. */
#define MAX_PENDING_MESSAGES (1024)

/** Max number of unprocessed queue messages */
#define MAX_QUEUE_MESSAGES (32)
typedef uint32_t msg_flag_t;

/** Special value meaning poll should block indefinitely. */
#define INFINITE_DELAY (-1)

/** Sentinel values used for listener.shutdown_notify_fd. */
#define LISTENER_NO_FD (-1)
#define LISTENER_SHUTDOWN_COMPLETE_FD (-2)

/** Receiver of responses */
typedef struct listener {
    struct bus *bus;

    /* File descriptor which should receive a notification when the
     * listener is shutting down. It will be set to LISTENER_NO_FD
     * until shutdown is requested, then the FD to notify will be
     * saved, then the FD will be notifed and it will be set to
     * LISTENER_SHUTDOWN_COMPLETE_FD. */
    int shutdown_notify_fd;

    /* Pipes used to wake the sleeping listener on queue input. */
    int commit_pipe;
    int incoming_msg_pipe;
    bool is_idle;

    rx_info_t rx_info[MAX_PENDING_MESSAGES];
    rx_info_t *rx_info_freelist;
    uint16_t rx_info_in_use;
    uint16_t rx_info_max_used;

    listener_msg msgs[MAX_QUEUE_MESSAGES];
    listener_msg *msg_freelist;
    int16_t msgs_in_use;
    int64_t largest_seq_id_seen;

    size_t upstream_backpressure;

    uint16_t tracked_fds;       ///< FDs currently tracked by listener
    /** File descriptors that are inactive due to errors, but have not
     * yet been explicitly removed/closed by the client. */
    uint16_t inactive_fds;

    /** Tracked file descriptors, for polling.
     *
     * fds[INCOMING_MSG_PIPE_ID (0)] is the incoming_msg_pipe, so the
     * listener's poll is awakened by incoming commands. fds[1] through
     * fds[l->tracked_fds - l->inactive_fds] are the file descriptors
     * which should be polled, and the remaining ones (if any) have been
     * moved to the end so poll() will not touch them. */
    struct pollfd fds[MAX_FDS + 1];

    /** The connection info, corresponding to the the file descriptors tracked in
     * l->fds. Unlike l->fds, these are not offset by one for the incoming message
     * pipe, i.e. l->fd_info[3] correspons to l->fds[3 + INCOMING_MSG_PIPE]. */
    connection_info *fd_info[MAX_FDS];

    bool error_occured;         ///< Flag indicating post-poll handling is necessary.

    /* Read buffer and it's size. Will be grown on demand. */
    size_t read_buf_size;
    uint8_t *read_buf;
} listener;

#endif
