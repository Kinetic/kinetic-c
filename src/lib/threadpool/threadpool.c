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
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <poll.h>
#include <errno.h>

#include "threadpool_internals.h"

#define MIN_DELAY 10 /* msec */
#define DEFAULT_MAX_DELAY 10000 /* msec */
#define INFINITE_DELAY -1 /* poll will only return upon an event */
#define DEFAULT_TASK_RINGBUF_SIZE2 8
#define DEFAULT_MAX_THREADS 8

static void notify_new_task(struct threadpool *t);
static bool notify_shutdown(struct threadpool *t);
static bool spawn(struct threadpool *t);
static void *thread_task(void *thread_info);
static void commit_current_task(struct threadpool *t, struct marked_task *task, size_t wh);
static void release_current_task(struct threadpool *t, struct marked_task *task, size_t rh);

static void set_defaults(struct threadpool_config *cfg) {
    if (cfg->task_ringbuf_size2 == 0) {
        cfg->task_ringbuf_size2 = DEFAULT_TASK_RINGBUF_SIZE2;
    }

    if (cfg->max_threads == 0) { cfg->max_threads = DEFAULT_MAX_THREADS; }
}

struct threadpool *Threadpool_Init(struct threadpool_config *cfg) {
    set_defaults(cfg);

    if (cfg->task_ringbuf_size2 > THREADPOOL_MAX_RINGBUF_SIZE2) {
        return NULL;
    }
    if (cfg->max_threads < 1) { return NULL; }

    struct threadpool *t = NULL;
    struct marked_task *tasks = NULL;
    struct thread_info *threads = NULL;

    t = malloc(sizeof(*t));
    if (t == NULL) { goto cleanup; }

    size_t tasks_sz = (1 << cfg->task_ringbuf_size2) * sizeof(*tasks);
    size_t threads_sz = cfg->max_threads * sizeof(struct thread_info);

    tasks = malloc(tasks_sz);
    if (tasks == NULL) { goto cleanup; }

    threads = malloc(threads_sz);
    if (threads == NULL) { goto cleanup; }

    memset(t, 0, sizeof(*t));
    memset(threads, 0, threads_sz);

    /* Note: tasks is memset to a non-0 value so that the first slot,
     * tasks[0].mark, will not match its ID and leave it in a
     * prematurely commit-able state. */
    memset(tasks, 0xFF, tasks_sz);

    t->tasks = tasks;
    t->threads = threads;
    t->task_ringbuf_size = 1 << cfg->task_ringbuf_size2;
    t->task_ringbuf_size2 = cfg->task_ringbuf_size2;
    t->task_ringbuf_mask = t->task_ringbuf_size - 1;
    t->max_threads = cfg->max_threads;
    return t;

cleanup:
    if (t) { free(t); }
    if (tasks) { free(tasks); }
    if (threads) { free(threads); }
    return NULL;
}

bool Threadpool_Schedule(struct threadpool *t, struct threadpool_task *task,
        size_t *pushback) {
    if (t == NULL) { return false; }
    if (task == NULL || task->task == NULL) { return false; }

    /* New tasks must not be scheduled after the threadpool starts
     * shutting down. */
    if (t->shutting_down) { return false; }

    size_t queue_size = t->task_ringbuf_size - 1;
    size_t mask = queue_size;

    for (;;) {
        size_t wh = t->task_reserve_head;
        size_t rh = t->task_release_head;

        if (wh - rh >= queue_size - 1) {
            if (pushback) { *pushback = wh - rh; }
            //printf("FULL, %zd, %zd\n", wh - rh, t->task_commit_head - t->task_request_head);
            return false;       /* full, cannot schedule */
        }
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_reserve_head, wh, wh + 1)) {
            assert(t->task_reserve_head - t->task_release_head < queue_size);
            struct marked_task *tbuf = &t->tasks[wh & mask];
            tbuf->task = task->task;
            tbuf->cleanup = task->cleanup;
            tbuf->udata = task->udata;

            commit_current_task(t, tbuf, wh);
            notify_new_task(t);
            if (pushback) { *pushback = wh - rh; }
            return true;
        }
    }
}

static void commit_current_task(struct threadpool *t, struct marked_task *task, size_t wh) {
    size_t mask = t->task_ringbuf_mask;
    task->mark = wh;
    for (;;) {
        size_t ch = t->task_commit_head;
        task = &t->tasks[ch & mask];
        if (ch != task->mark) { break; }
        assert(ch < t->task_reserve_head);
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_commit_head, ch, ch + 1)) {
            assert(t->task_request_head <= t->task_commit_head);
        }
    }
}

void Threadpool_Stats(struct threadpool *t, struct threadpool_info *info) {
    if (info) {
        uint8_t at = 0;
        for (int i = 0; i < t->live_threads; i++) {
            struct thread_info *ti = &t->threads[i];
            if (ti->status == STATUS_AWAKE) { at++; }
        }
        info->active_threads = at;

        info->dormant_threads = t->live_threads - at;
        info->backlog_size = t->task_commit_head - t->task_request_head;
    }
}

bool Threadpool_Shutdown(struct threadpool *t, bool kill_all) {
    t->shutting_down = true;
    size_t mask = t->task_ringbuf_mask;

    if (kill_all) {
        for (int i = 0; i < t->live_threads; i++) {
            struct thread_info *ti = &t->threads[i];
            if (ti->status < STATUS_SHUTDOWN) {
                ti->status = STATUS_SHUTDOWN;
                int pcres = pthread_cancel(ti->t);
                if (pcres != 0) {
                    /* If this fails, tolerate the failure that the
                     * pthread has already shut down. */
                    assert(pcres == ESRCH);
                }
            }
        }
    }

    notify_shutdown(t);

    while (t->task_commit_head > t->task_request_head) {
        size_t rh = t->task_request_head;

        struct marked_task *tbuf = &t->tasks[rh & mask];
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_request_head, rh, rh + 1)) {
            if (tbuf->cleanup) {
                tbuf->cleanup(tbuf->udata);
                tbuf->udata = NULL;
            }
            SPIN_ADJ(t->task_release_head, 1);
        }
    }

    return notify_shutdown(t);
}

void Threadpool_Free(struct threadpool *t) {
    free(t->tasks);
    t->tasks = NULL;
    free(t->threads);
    t->threads = NULL;
    free(t);
}

static void notify_new_task(struct threadpool *t) {
    for (int i = 0; i < t->live_threads; i++) {
        struct thread_info *ti = &t->threads[i];
        if (ti->status == STATUS_ASLEEP) {
            ssize_t res = write(ti->parent_fd,
                NOTIFY_MSG, NOTIFY_MSG_LEN);
            if (NOTIFY_MSG_LEN == res) {
                return;
            } else if (res == -1) {
                err(1, "write");
            } else {
                ;
            }
        }
    }

    if (t->live_threads < t->max_threads) { /* spawn */
        if (spawn(t)) {
            SPIN_ADJ(t->live_threads, 1);
        }
    } else {
        /* all awake & busy, just keep out of the way & let them work */
    }
}

static bool notify_shutdown(struct threadpool *t) {
    int done = 0;

    for (int i = 0; i < t->live_threads; i++) {
        struct thread_info *ti = &t->threads[i];
        if (ti->status == STATUS_JOINED) {
            done++;
        } else if (ti->status == STATUS_SHUTDOWN) {
            void *v = NULL;
            int joinres = pthread_join(ti->t, &v);
            if (0 == joinres) {
                ti->status = STATUS_JOINED;
                done++;
            } else {
                fprintf(stderr, "pthread_join: %d\n", joinres);
                assert(joinres == ESRCH);
            }
        } else {
            close(ti->parent_fd);
        }
    }

    return (done == t->live_threads);
}

static bool spawn(struct threadpool *t) {
    int id = t->live_threads;
    if (id >= t->max_threads) { return false; }
    struct thread_info *ti = &t->threads[id];

    struct thread_context *tc = malloc(sizeof(*tc));
    if (tc == NULL) { return false; }

    int pipe_fds[2];
    if (0 != pipe(pipe_fds)) {
        printf("pipe(2) failure\n");
        free(tc);
        return false;
    }

    ti->child_fd = pipe_fds[0];
    ti->parent_fd = pipe_fds[1];

    *tc = (struct thread_context){ .t = t, .ti = ti };

    int res = pthread_create(&ti->t, NULL, thread_task, tc);
    if (res == 0) {
        ti->status = STATUS_AWAKE;
        return true;
    } else if (res == EAGAIN) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        free(tc);
        return false;
    } else {
        assert(false);
    }
}

static void *thread_task(void *arg) {
    struct thread_context *tc = (struct thread_context *)arg;
    struct threadpool *t = tc->t;
    struct thread_info *ti = tc->ti;

    size_t mask = t->task_ringbuf_mask;
    struct pollfd pfd[1] = { { .fd=ti->child_fd, .events=POLLIN }, };
    uint8_t read_buf[NOTIFY_MSG_LEN*32];

    while (ti->status < STATUS_SHUTDOWN) {
        if (t->task_request_head == t->task_commit_head) {
            if (ti->status == STATUS_AWAKE) {
                ti->status = STATUS_ASLEEP;
            }
            int res = poll(pfd, 1, -1);
            if (res == 1) {
                if (pfd[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    /* TODO: HUP should be distinct from ERR -- hup is
                     * intentional shutdown, ERR probably isn't. */
                    ti->status = STATUS_SHUTDOWN;
                    break;
                } else if (pfd[0].revents & POLLIN) {
                    if (ti->status == STATUS_ASLEEP) { ti->status = STATUS_AWAKE; }
                    //SPIN_ADJ(t->active_threads, 1);
                    ssize_t rres = read(ti->child_fd, read_buf, sizeof(read_buf));
                    if (rres < 0) {
                        assert(0);
                    }
                }
            }
        }

        while (ti->status < STATUS_SHUTDOWN) {
            size_t ch = t->task_commit_head;
            size_t rh = t->task_request_head;
            if (rh > ch - 1) {
                break;          /* nothing to do */
            }
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_request_head, rh, rh + 1)) {
                struct marked_task *ptask = &t->tasks[rh & mask];
                assert(ptask->mark == rh);

                struct marked_task task = {
                    .task = ptask->task,
                    .cleanup = ptask->cleanup,
                    .udata = ptask->udata,
                };

                release_current_task(t, ptask, rh);
                ptask = NULL;
                task.task(task.udata);
                break;
            }
        }
    }

    close(ti->child_fd);
    free(tc);
    return NULL;
}

static void release_current_task(struct threadpool *t, struct marked_task *task, size_t rh) {
    size_t mask = t->task_ringbuf_mask;
    task->mark = ~rh;
    for (;;) {
        size_t relh = t->task_release_head;
        task = &t->tasks[relh & mask];
        if (task->mark != ~relh) { break; }
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_release_head, relh, relh + 1)) {
            assert(relh < t->task_commit_head);
        }
    }
}
