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
#ifndef THREADPOOL_INTERNALS_H
#define THREADPOOL_INTERNALS_H

#include <pthread.h>
#include "threadpool.h"

/** Current status of a worker thread. */
typedef enum {
    STATUS_NONE,                //> undefined status
    STATUS_ASLEEP,              //> thread is poll-sleeping to reduce CPU
    STATUS_AWAKE,               //> thread is active
    STATUS_SHUTDOWN,            //> thread has been notified about shutdown
    STATUS_JOINED,              //> thread has been pthread_join'd
} thread_status_t;

/** Info retained by a thread while working. */
struct thread_info {
    pthread_t t;                //> thread
    int parent_fd;              //> alert pipe parent writes into
    int child_fd;               //> alert pipe child reads from
    thread_status_t status;     //> current worker thread status
};

/** Thread_info, plus pointer back to main threadpool manager. */
struct thread_context {
    struct threadpool *t;
    struct thread_info *ti;
};

/** A task, with an additional mark. */
struct marked_task {
    threadpool_task_cb *task;
    threadpool_task_cleanup_cb *cleanup;
    void *udata;
    /* This mark is used to indicate which tasks can have the commit_head
     * and release_head counters advanced past them.
     *
     * mark == (task_commit_head that points at tast): commit
     * mark == ~(task_release_head that points at tast): release
     *
     * Any other value means it is in an intermediate state and should be
     * retried later. */
    size_t mark;
};

/** Internal threadpool state. */
struct threadpool {
    /* reserve -> commit -> request -> release */
    size_t task_reserve_head;   //> reserved for write
    size_t task_commit_head;    //> done with write
    size_t task_request_head;   //> requested for read
    size_t task_release_head;   //> done processing task, can be overwritten

    struct marked_task *tasks;  //> ring buffer for tasks

    /* Size and mask. These can be derived from task_ringbuf_size2,
     * but are cached to reduce CPU. */
    size_t task_ringbuf_size;   //> size of ring buffer
    size_t task_ringbuf_mask;   //> mask to fit counter within ring buffer
    uint8_t task_ringbuf_size2; //> log2 of size of ring buffer

    bool shutting_down;         //> shutdown has been called
    uint8_t live_threads;       //> currently live threads
    uint8_t max_threads;        //> max number of threads to start
    struct thread_info *threads;
};

/* Do an atomic compare-and-swap, changing *PTR from OLD to NEW. Returns
 * true if the swap succeeded, false if it failed (generally because
 * another thread updated the memory first). */
#define ATOMIC_BOOL_COMPARE_AND_SWAP(PTR, OLD, NEW)     \
    (__sync_bool_compare_and_swap(PTR, OLD, NEW))

/* Message sent to wake up a thread. The message contents are currently unimportant. */
#define NOTIFY_MSG "!"
#define NOTIFY_MSG_LEN 1

/* Spin attempting to atomically adjust F by ADJ until successful. */
#define SPIN_ADJ(F, ADJ)                                                \
    do {                                                                \
        for (;;) {                                                      \
            size_t v = F;                                               \
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&F, v, v + ADJ)) {         \
                break;                                                  \
            }                                                           \
        }                                                               \
    } while (0)

#endif
