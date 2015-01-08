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
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* More than a billion tasks in the backlog is likely an error. */
#define THREADPOOL_MAX_RINGBUF_SIZE2 30

/* Opaque handle to threadpool. */
struct threadpool;

/* Configuration for thread pool. */
struct threadpool_config {
    uint8_t task_ringbuf_size2; /* log2(size) of task ring buffer */
    size_t max_delay;           /* max delay, in msec. 0 => default */
    uint8_t max_threads;        /* max threads to alloc on demand */
};

/* Callback for a task, with an arbitrary user-supplied pointer. */
typedef void (threadpool_task_cb)(void *udata);

/* Callback to clean up a cancelled task's data, with the arbitrary
 * user-supplied pointer that would have gone to the task. */
typedef void (threadpool_task_cleanup_cb)(void *udata);

/* A task. */
struct threadpool_task {
    threadpool_task_cb *task;
    threadpool_task_cleanup_cb *cleanup;
    void *udata;
};

/* Statistics about the current state of the threadpool. */
struct threadpool_info {
    uint8_t active_threads;
    uint8_t dormant_threads;
    size_t backlog_size;
};

/* Initialize a threadpool, according to a config. Returns NULL on error. */
struct threadpool *threadpool_init(struct threadpool_config *cfg);

/* Schedule a task in the threadpool. Returns whether the task was successfully
 * registered or not.
 *
 * If *pushback is non-NULL, it will be set to the number of tasks
 * in the backlog, so code upstream can provide counterpressure.
 *
 * TASK is copied into the threadpool by value. */
bool threadpool_schedule(struct threadpool *t, struct threadpool_task *task,
    size_t *pushback);

/* If TI is non-NULL, fill out some statistics about the operating state
 * of the thread pool. */
void threadpool_stats(struct threadpool *t, struct threadpool_info *ti);

/* Notify the threadpool's threads that the system is going to shut down soon.
 * Returns whether all previously active threads have shut down.
 *
 * kill_all will eventually permit immediately shutting down all active threads
 * (possibly with memory leaks), but is not yet implemented.
 *
 * Returns whether everything has halted. */
bool threadpool_shutdown(struct threadpool *t, bool kill_all);

/* Free a threadpool. Assumes either threadpool_shutdown() has been
 * repeatedly called already, or leaking memory and other resources
 * is acceptable. */
void threadpool_free(struct threadpool *t);

#endif
