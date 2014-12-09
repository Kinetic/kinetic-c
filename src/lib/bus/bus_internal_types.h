#ifndef BUS_INTERNAL_TYPES_H
#define BUS_INTERNAL_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "bus.h"

/* Struct for a message that will be passed from sender to listener to
 * threadpool, proceeding directly to the threadpool if there is an error
 * along the way. This must only have a single owner at a time. */
typedef struct boxed_msg {

    /* Result message, constructed in place after the request/response cycle
     * has completed or failed due to timeout / unrecoverable error. */
    bus_msg_result_t result;

    /* Callback and userdata to which the bus_msg_result_t above will be sunk. */
    bus_msg_cb *cb;
    void *udata;

    /* Destination filename and message body. */
    int fd;
    int64_t out_seq_id;
    uint8_t *out_msg;
    size_t out_msg_size;
} boxed_msg;

/* Message bus. */
typedef struct bus {
    bus_sink_cb *sink_cb;
    bus_unpack_cb *unpack_cb;
    bus_unexpected_msg_cb *unexpected_msg_cb;
    void *udata;

    int log_level;
    bus_log_cb *log_cb;
    pthread_mutex_t log_lock;

    uint8_t sender_count;
    struct sender **senders;

    uint8_t listener_count;
    struct listener **listeners;

    bool *joined;
    pthread_t *threads;

    struct threadpool *threadpool;
} bus;

/* Special timeout value indicating UNBOUND. */
#define TIMEOUT_NOT_YET_SET ((time_t)(-1))

typedef enum {
    RX_ERROR_NONE = 0,
    RX_ERROR_READY_FOR_DELIVERY = 1,
    RX_ERROR_DONE = 2,
    RX_ERROR_POLLHUP = -1,
    RX_ERROR_POLLERR = -2,
    RX_ERROR_READ_FAILURE = -3,
    RX_ERROR_TIMEOUT = -4,
} rx_error_t;

/* Per-socket connection context. (Owned by the listener.) */
typedef struct {
    int fd;
    rx_error_t error;

    size_t to_read_size;

    /* Read buffer and it's size. Will be grown on demand. */
    size_t read_buf_size;
    uint8_t *read_buf;

    void *udata;                /* user connection data */
} connection_info;

#endif
