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
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <poll.h>
#include <errno.h>

#include "threadpool_internals.h"

#define DEFAULT_MAX_DELAY 1000   /* msec */
#define DEFAULT_TASK_RINGBUF_SIZE2 8
#define DEFAULT_MAX_THREADS 8

static void set_defaults(struct threadpool_config *cfg) {
    if (cfg->task_ringbuf_size2 == 0) {
        cfg->task_ringbuf_size2 = DEFAULT_TASK_RINGBUF_SIZE2;
    }
    
    if (cfg->max_delay == 0) { cfg->max_delay = DEFAULT_MAX_DELAY; }
    if (cfg->max_threads == 0) { cfg->max_threads = DEFAULT_MAX_THREADS; }
}

struct threadpool *threadpool_init(struct threadpool_config *cfg) {
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
    t->max_delay = cfg->max_delay;
    return t;

cleanup:
    if (t) { free(t); }
    if (tasks) { free(tasks); }
    if (threads) { free(threads); }
    return NULL;
}

bool threadpool_schedule(struct threadpool *t, struct threadpool_task *task,
        size_t *pushback) {
    if (t == NULL) { return false; }
    if (task == NULL || task->task == NULL) { return false; }

    //size_t queue_size = (1 << t->task_ringbuf_size2) - 1;
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

            notify_new_task(t);
            if (pushback) { *pushback = wh - rh; }

            commit_current_task(t, tbuf, wh);
            //printf("delta %zd\n", wh);
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

void threadpool_stats(struct threadpool *t, struct threadpool_info *ti) {
    if (ti) {
        uint8_t at = 0;
        for (int i = 0; i < t->live_threads; i++) {
            struct thread_info *ti = &t->threads[i];
            if (ti->status == STATUS_AWAKE) { at++; }
        }
        ti->active_threads = at;

        ti->dormant_threads = t->live_threads - at;
        ti->backlog_size = t->task_commit_head - t->task_request_head;
    }
}

bool threadpool_shutdown(struct threadpool *t, bool kill_all) {
    size_t mask = t->task_ringbuf_mask;

    if (kill_all) {
        for (int i = 0; i < t->live_threads; i++) {
            struct thread_info *ti = &t->threads[i];
            if (ti->status < STATUS_SHUTDOWN) {
                ti->status = STATUS_SHUTDOWN;
                if (0 != pthread_cancel(ti->t)) {
                    assert(false);
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
                memset(tbuf, 0, sizeof(*tbuf));
            }
            SPIN_ADJ(t->task_release_head, 1);
        }
    }

    return notify_shutdown(t);
}

void threadpool_free(struct threadpool *t) {
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
            if (2 == res) {
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
            if (pthread_join(ti->t, &v)) {
                ti->status = STATUS_JOINED;
                done++;
            }
        } else {
            close(ti->parent_fd);
        }
    }
    
    return (done == t->live_threads);
}

static bool spawn(struct threadpool *t) {
    struct thread_info *ti = &t->threads[t->live_threads];

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
    uint8_t read_buf[NOTIFY_MSG_LEN];

    size_t delay = 1;

    for (;;) {
        if (t->task_request_head == t->task_commit_head) {
            if (ti->status == STATUS_AWAKE) {
                if (delay > 1) { ti->status = STATUS_ASLEEP; }
            } else {
                if (delay == 0) {
                    delay = 1;
                } else {
                    delay <<= 1;
                }
                if (delay > t->max_delay) {
                    delay = t->max_delay;
                }
            }

            int res = poll(pfd, 1, delay);
            if (res == 1) {
                if ((pfd[0].revents & POLLHUP) || (pfd[0].revents & POLLERR)) {
                    /* TODO: HUP should be distinct from ERR -- hup is
                     * intentional shutdown, ERR probably isn't. */
                    ti->status = STATUS_SHUTDOWN;
                    close(ti->child_fd);
                    free(tc);
                    return NULL;
                } else if (pfd[0].revents & POLLIN) {
                    if (ti->status == STATUS_ASLEEP) { ti->status = STATUS_AWAKE; }
                    delay = 0;
                    //SPIN_ADJ(t->active_threads, 1);
                    ssize_t rres = read(ti->child_fd, read_buf, NOTIFY_MSG_LEN);
                    if (rres < 0) {
                        assert(0);
                    }
                }
            }
        }

        for (;;) {
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
