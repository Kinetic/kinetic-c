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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* More than a billion tasks in the backlog is likely an error. */
#define THREADPOOL_MAX_RINGBUF_SIZE2 30

/** Opaque handle to threadpool. */
struct threadpool;

/** Configuration for thread pool. */
struct threadpool_config {
    uint8_t task_ringbuf_size2; //> log2(size) of task ring buffer
    size_t max_delay;           //> max delay, in msec. 0 => default
    uint8_t max_threads;        //> max threads to alloc on demand
};

/** Callback for a task, with an arbitrary user-supplied pointer. */
typedef void (threadpool_task_cb)(void *udata);

/** Callback to clean up a cancelled task's data, with the arbitrary
 * user-supplied pointer that would have gone to the task. */
typedef void (threadpool_task_cleanup_cb)(void *udata);

/** A task. */
struct threadpool_task {
    threadpool_task_cb *task;   //> Task function pointer.
    threadpool_task_cleanup_cb *cleanup;  //> Cleanup function pointer.
    void *udata;                //> User data to pass along to function pointer.
};

/** Statistics about the current state of the threadpool. */
struct threadpool_info {
    uint8_t active_threads;
    uint8_t dormant_threads;
    size_t backlog_size;
};

/** Initialize a threadpool, according to a config. Returns NULL on error. */
struct threadpool *Threadpool_Init(struct threadpool_config *cfg);

/** Schedule a task in the threadpool. Returns whether the task was successfully
 * registered or not. If Threadpool_Shutdown has been called, this
 * function will always return false, due to API misuse.
 *
 * If *pushback is non-NULL, it will be set to the number of tasks
 * in the backlog, so code upstream can provide counterpressure.
 *
 * TASK is copied into the threadpool by value. */
bool Threadpool_Schedule(struct threadpool *t, struct threadpool_task *task,
    size_t *pushback);

/** If TI is non-NULL, fill out some statistics about the operating state
 * of the thread pool. */
void Threadpool_Stats(struct threadpool *t, struct threadpool_info *ti);

/** Notify the threadpool's threads that the system is going to shut down soon.
 * Returns whether all previously active threads have shut down.
 *
 * kill_all will eventually permit immediately shutting down all active threads
 * (possibly with memory leaks), but is not yet implemented.
 *
 * Returns whether everything has halted. */
bool Threadpool_Shutdown(struct threadpool *t, bool kill_all);

/** Free a threadpool. Assumes either Threadpool_Shutdown() has been
 * repeatedly called already, or leaking memory and other resources
 * is acceptable. */
void Threadpool_Free(struct threadpool *t);

#ifdef TEST
#include "threadpool_internals.h"
#endif

#endif
