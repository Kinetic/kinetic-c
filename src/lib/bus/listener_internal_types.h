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
#ifndef LISTENER_INTERNAL_TYPES_H
#define LISTENER_INTERNAL_TYPES_H

#include "bus_types.h"
#include "bus_internal_types.h"

#include <poll.h>

#define DEFAULT_READ_BUF_SIZE (1024L * 1024L)
#define INCOMING_MSG_PIPE 1
#define INCOMING_MSG_PIPE_ID 0

typedef enum {
    MSG_NONE,
    MSG_ADD_SOCKET,
    MSG_REMOVE_SOCKET,
    MSG_HOLD_RESPONSE,
    MSG_EXPECT_RESPONSE,
    MSG_SHUTDOWN,
} MSG_TYPE;

/* Queue message. */
typedef struct listener_msg {
    const uint8_t id;
    MSG_TYPE type;
    struct listener_msg *next;
    int pipes[2];
    
    union {
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
        } hold;
        struct {
            boxed_msg *box;
        } expect;
        struct {
            int notify_fd;
        } shutdown;
    } u;
} listener_msg;

/* How long the listener should wait for responses before blocking. */
#define LISTENER_TASK_TIMEOUT_DELAY 100

typedef enum {
    RIS_HOLD = 1,
    RIS_EXPECT = 2,
    RIS_INACTIVE = 3,
} rx_info_state;

/* Record in table for partially processed messages. */
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

/* Max number of sockets to monitor. */
#define MAX_FDS 1000

/* Max number of partially processed messages.
 * TODO: Capacity planning. */
#define MAX_PENDING_MESSAGES (1024)

/* Max number of unprocessed queue messages */
#define MAX_QUEUE_MESSAGES 32
typedef uint32_t msg_flag_t;

/* Minimum and maximum poll() delays for listener, before going dormant. */
#define MIN_DELAY 10
#define MAX_DELAY 100
#define INFINITE_DELAY (-1)

/* Sentinel values used for listener.shutdown_notify_fd. */
#define LISTENER_NO_FD (-1)
#define LISTENER_SHUTDOWN_COMPLETE_FD (-2)

/* Receiver of responses */
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

    uint16_t tracked_fds;
    /* tracked_fds + incoming_msg_pipe */
    struct pollfd fds[MAX_FDS + 1];
    connection_info *fd_info[MAX_FDS];

    /* Read buffer and it's size. Will be grown on demand. */
    size_t read_buf_size;
    uint8_t *read_buf;
} listener;

#endif
