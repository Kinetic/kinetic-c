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

#define MAX_TIMEOUT 100
#define TX_TIMEOUT 9

#define SENDER_MAX_DELIVERY_RETRIES 5

typedef enum {
    TX_ERROR_NONE = 0,
    TX_ERROR_POLLHUP = -1,
    TX_ERROR_POLLERR = -2,
    TX_ERROR_WRITE_FAILURE = -3,
} tx_error_t;

/* Metadata for a message in-flight. */
typedef struct {
    /* Set during initialization only. */
    const int id;
    int fd;

    /* Should be either TIMEOUT_NOT_YET_SET or a number of seconds, which
     * will count down to 0, triggering a timeout event. */
    time_t timeout_sec;         /* relative countdown */

    /* Other fields that can be safely modified when timeout_sec is
     * TIMEOUT_NOT_YET_SET. */
    boxed_msg *box;

    /* Other internal state for the network write. */
    uint8_t retries;
    tx_error_t error;
    size_t sent_size;
} tx_info_t;

/* Max number of in-flight messagse. */
#define MAX_CONCURRENT_SENDS 32
typedef uint32_t tx_flag_t;

#define HASH_TABLE_SIZE2 6 /* should be > log2(MAX_CONCURRENT_SENDS) */

typedef struct sender {
    struct bus *bus;
    bool shutdown;

    /* Outgoing message data. */
    tx_flag_t tx_flags;
    tx_info_t tx_info[MAX_CONCURRENT_SENDS];
    int16_t tx_info_in_use;

    int pipes[MAX_CONCURRENT_SENDS][2];

    /* Number of *distinct* FDs we're monitoring for being writeable.
     * The mutex is used because any operations that change the set of
     * file descriptors that are being monitored for being writeable
     * need to be done atomically. */
    pthread_mutex_t watch_set_mutex;
    uint8_t active_fds;
    struct pollfd fds[MAX_CONCURRENT_SENDS];

    /* Hash table for file descriptors in use. */
    struct yacht *fd_hash_table;
} sender;

static tx_info_t *get_free_tx_info(struct sender *s);
static void tick_handler(sender *s);
static void enqueue_outgoing_msg(sender *s, void *boxed_msg);
static void shutdown(sender *s);
static bool populate_tx_info(struct sender *s, tx_info_t *info, boxed_msg *box);
static bool add_fd_to_watch_set(struct sender *s, int fd);
static bool remove_fd_from_watch_set(struct sender *s, tx_info_t *info);
static void attempt_write(sender *s, int available);

#endif
