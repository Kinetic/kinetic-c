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
#ifndef SENDER_INTERNAL_H
#define SENDER_INTERNAL_H

#include "sender.h"

#define MAX_TIMEOUT 100
#define TX_TIMEOUT 9

#define SENDER_MAX_DELIVERY_RETRIES 5

/* Max number of in-flight messagse. */
#define MAX_CONCURRENT_SENDS 32
typedef uint32_t tx_flag_t;

#define HASH_TABLE_SIZE2 6 /* should be > log2(MAX_CONCURRENT_SENDS) */

#define SENDER_FD_NOT_IN_USE (-1)
typedef enum {
    TX_ERROR_NONE = 0,
    TX_ERROR_POLLHUP = -11,
    TX_ERROR_POLLERR = -12,
    TX_ERROR_WRITE_FAILURE = -13,
    TX_ERROR_UNREGISTERED_SOCKET = -14,
    TX_ERROR_CLOSED = -15,
    TX_ERROR_NOTIFY_LISTENER_FAILURE = -16,
    TX_ERROR_WRITE_TIMEOUT = -17,
    TX_ERROR_NOTIFY_TIMEOUT = -18,
    TX_ERROR_BAD_SEQUENCE_ID = -19,
} tx_error_t;

typedef enum {
    TIS_UNDEF = 0,

    /* Single-step commands */
    TIS_ADD_SOCKET = 1,
    TIS_RM_SOCKET = 2,
    TIS_SHUTDOWN = 3,

    /* States for an outgoing request */
    TIS_REQUEST_ENQUEUE = 4,
    TIS_REQUEST_WRITE = 5,
    TIS_RESPONSE_NOTIFY = 6,
    TIS_REQUEST_RELEASE = 7,

    TIS_ERROR = -1,
} tx_info_state;

typedef struct {
    int fd;
    SSL *ssl;                   /* SSL handle. Can be NULL. */
    int refcount;
    int64_t largest_seq_id_seen;
    bool errored;
} fd_info;

/* Metadata for a message in-flight. */
typedef struct {
    /* Set during initialization only. */
    const int id;

    /* Pipe written to notify event completion. */
    int done_pipe;

    /* Current state tag for union */
    tx_info_state state;
    union {
        struct {
            int fd;
            SSL *ssl;
        } add_socket;

        struct {
            int fd;
        } rm_socket;

        struct u_enqueue {
            int fd;
            time_t timeout_sec;
            boxed_msg *box;
        } enqueue;

        struct u_write {
            int fd;
            time_t timeout_sec;
            boxed_msg *box;
            size_t sent_size;
            uint8_t retries;
            fd_info *fdi;
        } write;

        struct u_notify {
            int fd;
            time_t timeout_sec;
            boxed_msg *box;
            uint8_t retries;
        } notify;

        struct {
            uint16_t backpressure;
        } release;

        struct u_error {
            tx_error_t error;
            boxed_msg *box;
            uint16_t backpressure;
        } error;
    } u;
} tx_info_t;

typedef struct sender {
    struct bus *bus;
    bool shutdown;

    /* Internal structure for pending events.
     *
     * These are reserved atomically by the client thread (which blocks
     * until they are handled), using a CAS on (tx_flags << tx_info->id)
     * to set/clear bits. */
    tx_flag_t tx_flags;
    tx_info_t tx_info[MAX_CONCURRENT_SENDS];

    int pipes[MAX_CONCURRENT_SENDS][2];

    /* Pipe which gets tx_info IDs written to it to notify that
     * commands have been committed. */
    int commit_pipe;
    int incoming_command_pipe;

    int64_t largest_seq_id_seen;

    /* Set of file descriptors to poll.
     * fds[0] is the incoming command pipe, and is polled for
     * read; the rest (fds[1] through fds[1 + active_fds] are
     * sockets with pending outgoing requests, and are polled
     * for write. */
    uint8_t active_fds;
    struct pollfd fds[MAX_CONCURRENT_SENDS + 1];

    /* Hash table for file descriptors in use -> fd_info.
     * This tracks which file descriptors are registered
     * and whether they use SSL. */
    struct yacht *fd_hash_table;
} sender;

static tx_info_t *get_free_tx_info(struct sender *s);
static void release_tx_info(struct sender *s, tx_info_t *info);
static int get_notify_pipe(struct sender *s, int id);
static bool write_commit(struct sender *s, tx_info_t *info);
static tx_error_t commit_event_and_block(struct sender *s, tx_info_t *info);

static bool register_socket_info(sender *s, int fd, SSL *ssl);
static void increment_fd_refcount(sender *s, fd_info *fdi);
static void decrement_fd_refcount(sender *s, fd_info *fdi);
static bool release_socket_info(sender *s, int fd);
static void handle_command(sender *s, int id);
static void enqueue_write(struct sender *s, tx_info_t *info);
static bool check_incoming_commands(struct sender *s);

static tx_info_t *get_info_to_write_for_socket(sender *s, int fd);
static void set_error_for_socket(sender *s, int fd, tx_error_t error);

static ssize_t socket_write_plain(sender *s, tx_info_t *info);
static ssize_t socket_write_ssl(sender *s, tx_info_t *info, SSL *ssl);
static void update_sent(struct bus *b, sender *s, tx_info_t *info, ssize_t sent);
static void attempt_write(sender *s, int available);

static void tick_handler(sender *s);
static void notify_message_failure(sender *s, tx_info_t *info, bus_send_status_t status);
static void attempt_to_enqueue_sending_request_message_to_listener(sender *s,
    int fd, int64_t seq_id, int16_t timeout_sec);
static void attempt_to_enqueue_request_sent_message_to_listener(sender *s, tx_info_t *info);

static void notify_caller(sender *s, tx_info_t *info, bool success);
static void tick_timeout(sender *s, tx_info_t *info);
static void cleanup(sender *s);

#endif
