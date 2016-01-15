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
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <sys/resource.h>

#include "bus.h"
#include "bus_poll.h"
#include "send.h"
#include "listener.h"
#include "threadpool.h"
#include "bus_internal_types.h"
#include "bus_ssl.h"
#include "util.h"
#include "yacht.h"
#include "syscall.h"
#include "atomic.h"

#include "kinetic_types_internal.h"
#include "listener_task.h"

static int listener_id_of_socket(struct bus *b, int fd);
static void noop_log_cb(log_event_t event,
        int log_level, const char *msg, void *udata);
static void noop_error_cb(bus_unpack_cb_res_t result, void *socket_udata);
static bool attempt_to_increase_resource_limits(struct bus *b);

static void set_defaults(bus_config *cfg) {
    if (cfg->listener_count == 0) { cfg->listener_count = 1; }
}

#ifdef TEST
boxed_msg *test_box = NULL;
void *value = NULL;
void *old_value = NULL;
connection_info *test_ci = NULL;
int completion_pipe = -1;
void *unused = NULL;
#endif

bool Bus_Init(bus_config *config, struct bus_result *res) {
    if (res == NULL) { return false; }
    if (config == NULL) {
        res->status = BUS_INIT_ERROR_NULL;
        return false;
    }
    set_defaults(config);
    if (config->sink_cb == NULL) {
        res->status = BUS_INIT_ERROR_MISSING_SINK_CB;
        return false;
    }
    if (config->unpack_cb == NULL) {
        res->status = BUS_INIT_ERROR_MISSING_UNPACK_CB;
        return false;
    }
    if (config->log_cb == NULL) {
        config->log_cb = noop_log_cb;
        config->log_level = INT_MIN;
    }
    if (config->error_cb == NULL) {
        config->error_cb = noop_error_cb;
    }

    res->status = BUS_INIT_ERROR_ALLOC_FAIL;

    uint8_t locks_initialized = 0;
    struct listener **ls = NULL;     /* listeners */
    struct threadpool *tp = NULL;
    bool *joined = NULL;
    pthread_t *threads = NULL;
    struct yacht *fds = NULL;

    bus *b = calloc(1, sizeof(*b));
    if (b == NULL) { goto cleanup; }

    if (!BusSSL_Init(b)) { goto cleanup; }

    b->sink_cb = config->sink_cb;
    b->unpack_cb = config->unpack_cb;
    b->unexpected_msg_cb = config->unexpected_msg_cb;
    b->error_cb = config->error_cb;
    b->log_cb = config->log_cb;
    b->log_level = config->log_level;
    b->udata = config->bus_udata;
    if (0 != pthread_mutex_init(&b->fd_set_lock, NULL)) {
        res->status = BUS_INIT_ERROR_MUTEX_INIT_FAIL;
        goto cleanup;
    }
    locks_initialized++;

    attempt_to_increase_resource_limits(b);

    BUS_LOG_SNPRINTF(b, 3, LOG_INITIALIZATION, b->udata, 64,
        "Initialized bus at %p", (void*)b);

    ls = calloc(config->listener_count, sizeof(*ls));
    if (ls == NULL) {
        goto cleanup;
    }

    for (int i = 0; i < config->listener_count; i++) {
        ls[i] = Listener_Init(b, config);
        if (ls[i] == NULL) {
            res->status = BUS_INIT_ERROR_LISTENER_INIT_FAIL;
            goto cleanup;
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_INITIALIZATION, b->udata, 64,
                "Initialized listener %d at %p", i, (void*)ls[i]);
        }
    }

    tp = Threadpool_Init(&config->threadpool_cfg);
    if (tp == NULL) {
        res->status = BUS_INIT_ERROR_THREADPOOL_INIT_FAIL;
        goto cleanup;
    }

    int thread_count = config->listener_count;
    joined = calloc(thread_count, sizeof(bool));
    threads = calloc(thread_count, sizeof(pthread_t));
    if (joined == NULL || threads == NULL) {
        goto cleanup;
    }

    fds = Yacht_Init(DEF_FD_SET_SIZE2);
    if (fds == NULL) {
        goto cleanup;
    }

    b->listener_count = config->listener_count;
    b->listeners = ls;
    b->threadpool = tp;
    b->joined = joined;
    b->threads = threads;

    for (int i = 0; i < b->listener_count; i++) {
        int pcres = pthread_create(&b->threads[i], NULL,
            ListenerTask_MainLoop, (void *)b->listeners[i]);
        if (pcres != 0) {
            res->status = BUS_INIT_ERROR_PTHREAD_INIT_FAIL;
            goto cleanup;
        }
    }

    b->fd_set = fds;
    res->bus = b;
    BUS_LOG(b, 2, LOG_INITIALIZATION, "initialized", config->bus_udata);
    return true;

cleanup:
    if (ls) {
        for (int i = 0; i < config->listener_count; i++) {
            if (ls[i]) { Listener_Free(ls[i]); }
        }
        free(ls);
    }
    if (tp) { Threadpool_Free(tp); }
    if (joined) { free(joined); }
    if (b) {
        if (locks_initialized > 1) {
            pthread_mutex_destroy(&b->fd_set_lock);
        }
        free(b);
    }

    if (threads) { free(threads); }
    if (fds) { Yacht_Free(fds, NULL, NULL); }

    return false;
}

static bool attempt_to_increase_resource_limits(struct bus *b) {
    struct rlimit info;
    if (-1 == getrlimit(RLIMIT_NOFILE, &info)) {
        fprintf(stderr, "getrlimit: %s", strerror(errno));
        errno = 0;
        return false;
    }

    const unsigned int nval = 1024;

    if (info.rlim_cur < nval && info.rlim_max > nval) {
        info.rlim_cur = nval;
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
            "Current FD resource limits, [%lu, %lu], changing to %u",
            (unsigned long)info.rlim_cur, (unsigned long)info.rlim_max, nval);
        if (-1 == setrlimit(RLIMIT_NOFILE, &info)) {
            BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
                "Failed to increase FD resource limit to %u, %s",
                nval, strerror(errno));
            fprintf(stderr, "getrlimit: %s", strerror(errno));
            errno = 0;
            return false;
        } else {
            BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
                "Successfully increased FD resource limit to %u", nval);
        }
    } else {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
            "Current FD resource limits [%lu, %lu] are acceptable",
            (unsigned long)info.rlim_cur, (unsigned long)info.rlim_max);
    }
    return true;
}

/* Pack message to deliver on behalf of the user into an envelope
 * that can track status / routing along the way.
 *
 * The box should only ever be accessible on a single thread at a time. */
static boxed_msg *box_msg(struct bus *b, bus_user_msg *msg) {
    boxed_msg *box = NULL;
    #ifdef TEST
    box = test_box;
    #else
    box = calloc(1, sizeof(*box));
    #endif
    if (box == NULL) { return NULL; }

    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 64,
        "Allocated boxed message -- %p", (void*)box);

    box->fd = msg->fd;
    assert(msg->fd != 0);

    /* Lock hash table and check whether this FD uses SSL. */
    if (0 != pthread_mutex_lock(&b->fd_set_lock)) { assert(false); }
#ifndef TEST
    void *value = NULL;
#endif
    connection_info *ci = NULL;
    if (Yacht_Get(b->fd_set, box->fd, &value)) {
        ci = (connection_info *)value;
    }
    if (0 != pthread_mutex_unlock(&b->fd_set_lock)) { assert(false); }

    if (ci == NULL) {
        /* socket isn't registered, fail out */
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 64,
            "socket isn't registered, failing -- %p", (void*)box);
        free(box);
        return NULL;
    } else {
        box->ssl = ci->ssl;
    }

    if ((msg->seq_id <= ci->largest_wr_seq_id_seen)
            && (ci->largest_wr_seq_id_seen != BUS_NO_SEQ_ID)) {
        BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 256,
            "rejecting request <fd:%d, seq_id:%lld> due to non-monotonic sequence ID, largest seen is %lld",
            box->fd, (long long)msg->seq_id, (long long)ci->largest_wr_seq_id_seen);
        free(box);
        return NULL;
    } else {
        ci->largest_wr_seq_id_seen = msg->seq_id;
    }

    box->timeout_sec = (time_t)msg->timeout_sec;
    if (box->timeout_sec == 0) {
        box->timeout_sec = BUS_DEFAULT_TIMEOUT_SEC;
    }

    box->out_seq_id = msg->seq_id;
    box->out_msg_size = msg->msg_size;

    /* Store message by pointer, since the client code calling in is
     * blocked until we are done sending. */
    box->out_msg = msg->msg;

    box->cb = msg->cb;
    box->udata = msg->udata;
    return box;
}

bool Bus_SendRequest(struct bus *b, bus_user_msg *msg)
{
    if (b == NULL || msg == NULL || msg->fd == -1) {
        return false;
    }

    boxed_msg *box = box_msg(b, msg);
    if (box == NULL) {
        return false;
    }

    BUS_LOG_SNPRINTF(b, 3-0, LOG_SENDING_REQUEST, b->udata, 64,
        "Sending request <fd:%d, seq_id:%lld>", msg->fd, (long long)msg->seq_id);
    bool res = Send_DoBlockingSend(b, box);
    BUS_LOG_SNPRINTF(b, 3, LOG_SENDING_REQUEST, b->udata, 64,
        "...request sent, result %d", res);

    /* The send was rejected -- free the box, but don't call the error
     * handling callback. */
    if (!res) {
        BUS_LOG_SNPRINTF(b, 3, LOG_SENDING_REQUEST, b->udata, 64,
            "Freeing box since request was rejected: %p", (void *)box);
        free(box);
    }

    return res;
}

static int listener_id_of_socket(struct bus *b, int fd) {
    /* Just evenly divide sockets between listeners by file descriptor. */
    return fd % b->listener_count;
}

struct listener *Bus_GetListenerForSocket(struct bus *b, int fd) {
    return b->listeners[listener_id_of_socket(b, fd)];
}

/* Get the string key for a log event ID. */
const char *Bus_LogEventStr(log_event_t event) {
    switch (event) {
    case LOG_INITIALIZATION: return "INITIALIZATION";
    case LOG_NEW_CLIENT: return "NEW_CLIENT";
    case LOG_SOCKET_REGISTERED: return "SOCKET_REGISTERED";
    case LOG_SENDING_REQUEST: return "SEND_REQUEST";
    case LOG_SHUTDOWN: return "SHUTDOWN";
    case LOG_SENDER: return "SENDER";
    case LOG_LISTENER: return "LISTENER";
    case LOG_MEMORY: return "MEMORY";
    default:
        return "UNKNOWN";
    }
}

bool Bus_RegisterSocket(struct bus *b, bus_socket_t type, int fd, void *udata) {
    /* Register a socket internally with the listener. */
    int l_id = listener_id_of_socket(b, fd);

    BUS_LOG_SNPRINTF(b, 2, LOG_SOCKET_REGISTERED, b->udata, 64,
        "registering socket %d", fd);

    /* Spread sockets throughout the different listener threads. */
    struct listener *l = b->listeners[l_id];

    /* Metadata about the connection. Note: This will be shared by the
     * client thread and the listener thread, but each will only modify
     * some of the fields. The client thread will free this. */
    #ifdef TEST
    connection_info *ci = test_ci;
    #else
    connection_info *ci = calloc(1, sizeof(*ci));
    #endif
    if (ci == NULL) { goto cleanup; }

    SSL *ssl = NULL;
    if (type == BUS_SOCKET_SSL) {
        ssl = BusSSL_Connect(b, fd);
        if (ssl == NULL) { goto cleanup; }
    } else {
        ssl = BUS_NO_SSL;
    }

    *(int *)&ci->fd = fd;
    *(bus_socket_t *)&ci->type = type;
    ci->ssl = ssl;
    ci->udata = udata;
    ci->largest_wr_seq_id_seen = BUS_NO_SEQ_ID;

    #ifndef TEST
    void *old_value = NULL;
    #endif
    /* Lock hash table and save whether this FD uses SSL. */
    if (0 != pthread_mutex_lock(&b->fd_set_lock)) { assert(false); }
    bool set_ok = Yacht_Set(b->fd_set, fd, ci, &old_value);
    if (0 != pthread_mutex_unlock(&b->fd_set_lock)) { assert(false); }

    if (set_ok) {
        assert(old_value == NULL);
    } else {
        goto cleanup;
    }

    bool res = false;
    #ifndef TEST
    int completion_pipe = -1;
    #endif
    res = Listener_AddSocket(l, ci, &completion_pipe);
    if (!res) { goto cleanup; }

    BUS_LOG(b, 2, LOG_SOCKET_REGISTERED, "polling on socket add...", b->udata);
    bool completed = BusPoll_OnCompletion(b, completion_pipe);
    if (!completed) { goto cleanup; }

    BUS_LOG(b, 2, LOG_SOCKET_REGISTERED, "successfully added socket", b->udata);
    return true;
cleanup:
    if (ci) {
        free(ci);
    }
    BUS_LOG(b, 2, LOG_SOCKET_REGISTERED, "failed to add socket", b->udata);
    return false;
}

/* Free metadata about a socket that has been disconnected. */
bool Bus_ReleaseSocket(struct bus *b, int fd, void **socket_udata_out) {
    int l_id = listener_id_of_socket(b, fd);

    BUS_LOG_SNPRINTF(b, 2, LOG_SOCKET_REGISTERED, b->udata, 64,
        "forgetting socket %d", fd);

    struct listener *l = b->listeners[l_id];

    #ifndef TEST
    int completion_pipe = -1;
    #endif
    if (!Listener_RemoveSocket(l, fd, &completion_pipe)) {
        return false;           /* couldn't send msg to listener */
    }

    assert(completion_pipe != -1);
    bool completed = BusPoll_OnCompletion(b, completion_pipe);
    if (!completed) {           /* listener hung up while waiting */
        return false;
    }

    /* Lock hash table and forget whether this FD uses SSL. */
    #ifndef TEST
    void *old_value = NULL;
    #endif
    if (0 != pthread_mutex_lock(&b->fd_set_lock)) { assert(false); }
    bool rm_ok = Yacht_Remove(b->fd_set, fd, &old_value);
    if (0 != pthread_mutex_unlock(&b->fd_set_lock)) { assert(false); }
    if (!rm_ok) {
        return false;
    }

    connection_info *ci = (connection_info *)old_value;
    assert(ci != NULL);

    if (socket_udata_out) { *socket_udata_out = ci->udata; }

    bool res = false;

    if (ci->ssl == BUS_NO_SSL) {
        res = true;            /* nothing else to do */
    } else {
        res = BusSSL_Disconnect(b, ci->ssl);
    }

    free(ci);
    return res;
}

#ifndef TEST
static
#endif
void free_connection_cb(void *value, void *udata) {
    struct bus *b = (struct bus *)udata;
    connection_info *ci = (connection_info *)value;

    int l_id = listener_id_of_socket(b, ci->fd);
    struct listener *l = b->listeners[l_id];

    #ifndef TEST
    int completion_pipe = -1;
    #endif
    if (!Listener_RemoveSocket(l, ci->fd, &completion_pipe)) {
        return;           /* couldn't send msg to listener */
    }

    bool completed = BusPoll_OnCompletion(b, completion_pipe);
    if (!completed) {
        return;
    }

    free(ci);
}

bool Bus_Shutdown(bus *b) {
    for (;;) {
        shutdown_state_t ss = b->shutdown_state;
        /* Another thread is already shutting things down. */
        if (ss != SHUTDOWN_STATE_RUNNING) { return false; }
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&b->shutdown_state,
                SHUTDOWN_STATE_RUNNING, SHUTDOWN_STATE_SHUTTING_DOWN)) {
            break;
        }
    }

    if (b->fd_set) {
        BUS_LOG(b, 2, LOG_SHUTDOWN, "removing all connections", b->udata);
        Yacht_Free(b->fd_set, free_connection_cb, b);
        b->fd_set = NULL;
    }

    #ifndef TEST
    int completion_pipe = -1;
    #endif

    BUS_LOG(b, 2, LOG_SHUTDOWN, "shutting down listener threads", b->udata);
    for (int i = 0; i < b->listener_count; i++) {
        if (!b->joined[i]) {
            BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
                "Listener_Shutdown -- %d", i);
            if (!Listener_Shutdown(b->listeners[i], &completion_pipe)) {
                b->shutdown_state = SHUTDOWN_STATE_RUNNING;
                return false;
            }

            if (!BusPoll_OnCompletion(b, completion_pipe)) {
                b->shutdown_state = SHUTDOWN_STATE_RUNNING;
                return false;
            }

            BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
                "Listener_Shutdown -- joining %d", i);
            #ifndef TEST
            void *unused = NULL;
            #endif
            int res = syscall_pthread_join(b->threads[i], &unused);
            BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
                "Listener_Shutdown -- joined %d", i);
            if (res != 0) {
                b->shutdown_state = SHUTDOWN_STATE_RUNNING;
                return false;
            }
            b->joined[i] = true;
        }
    }

    BUS_LOG(b, 2, LOG_SHUTDOWN, "done with shutdown", b->udata);
    b->shutdown_state = SHUTDOWN_STATE_HALTED;
    return true;
}

void Bus_BackpressureDelay(struct bus *b, size_t backpressure, uint8_t shift) {
    /* Push back if message bus is too busy. */
    backpressure >>= shift;

    if (backpressure > 0) {
        BUS_LOG_SNPRINTF(b, 8, LOG_SENDER, b->udata, 64,
            "backpressure %zd", backpressure);
        syscall_poll(NULL, 0, backpressure);
    }
}

static void box_execute_cb(void *udata) {
    boxed_msg *box = (boxed_msg *)udata;

    void *out_udata = box->udata;
    bus_msg_result_t res = box->result;
    bus_msg_cb *cb = box->cb;

    free(box);
    cb(&res, out_udata);
}

static void box_cleanup_cb(void *udata) {
    boxed_msg *box = (boxed_msg *)udata;
    free(box);
}

/* Deliver a boxed message to the thread pool to execute.
 * The boxed message will be freed by the threadpool. */
bool Bus_ProcessBoxedMessage(struct bus *b,
        struct boxed_msg *box, size_t *backpressure) {
    assert(box);
    assert(box->result.status != BUS_SEND_UNDEFINED);

    struct threadpool_task task = {
        .task = box_execute_cb,
        .cleanup = box_cleanup_cb,
        .udata = box,
    };

    BUS_LOG_SNPRINTF(b, 3, LOG_MEMORY, b->udata, 128,
        "Scheduling boxed message -- %p -- where it will be freed", (void*)box);
    return Threadpool_Schedule(b->threadpool, &task, backpressure);
}

/* How many seconds should it give the thread pool to shut down? */
#define THREAD_SHUTDOWN_SECONDS 5

void Bus_Free(bus *b) {
    if (b == NULL) { return; }
    while (b->shutdown_state != SHUTDOWN_STATE_HALTED) {
        if (Bus_Shutdown(b)) { break; }
        syscall_poll(NULL, 0, 10);  // sleep 10 msec
    }

    for (int i = 0; i < b->listener_count; i++) {
        BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
            "Listener_Free -- %d", i);
        Listener_Free(b->listeners[i]);
    }
    free(b->listeners);

    int limit = (1000 * THREAD_SHUTDOWN_SECONDS)/10;
    for (int i = 0; i < limit; i++) {
        BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
            "Threadpool_Shutdown -- %d", i);
        if (Threadpool_Shutdown(b->threadpool, false)) { break; }
        (void)syscall_poll(NULL, 0, 10);

        if (i == limit - 1) {
            BUS_LOG_SNPRINTF(b, 3, LOG_SHUTDOWN, b->udata, 128,
                "Threadpool_Shutdown -- %d (forced)", i);
            Threadpool_Shutdown(b->threadpool, true);
        }
    }
    BUS_LOG(b, 3, LOG_SHUTDOWN, "Threadpool_Free", b->udata);
    Threadpool_Free(b->threadpool);
    free(b->joined);
    free(b->threads);
    pthread_mutex_destroy(&b->fd_set_lock);

    BusSSL_CtxFree(b);
    free(b);
}

static void noop_log_cb(log_event_t event,
        int log_level, const char *msg, void *udata) {
    (void)event;
    (void)log_level;
    (void)msg;
    (void)udata;
}

static void noop_error_cb(bus_unpack_cb_res_t result, void *socket_udata) {
    (void)result;
    (void)socket_udata;
}
