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

#include "threadpool.h"

typedef enum {
    STATUS_NONE,                /* undefined status */
    STATUS_ASLEEP,              /* thread is poll-sleeping to reduce CPU */
    STATUS_AWAKE,               /* thread is active */
    STATUS_SHUTDOWN,            /* thread has been notified about shutdown */
    STATUS_JOINED,              /* thread has been pthread_join'd */
} thread_status_t;

/* Info retained by a thread while working. */
struct thread_info {
    pthread_t t;
    int parent_fd;
    int child_fd;
    thread_status_t status;
};

/* Thread_info, plus pointer back to main threadpool manager. */
struct thread_context {
    struct threadpool *t;
    struct thread_info *ti;
};

/* Internal threadpool state */
struct threadpool {
    /* reserve -> commit -> request -> release */
    size_t task_reserve_head;   /* reserved for write */
    size_t task_commit_head;    /* done with write */
    size_t task_request_head;   /* requested for read */
    size_t task_release_head;   /* done processing task, can overwrite */

    struct threadpool_task *tasks;
    /* user stats */
    size_t tasks_assigned;
    size_t tasks_completed;

    size_t max_delay;           /* ceil of exponential sleep back-off */

    uint8_t task_ringbuf_size2;
    uint8_t max_threads;

    uint8_t active_threads;
    uint8_t live_threads;
    struct thread_info *threads;
};

#define ATOMIC_BOOL_COMPARE_AND_SWAP(PTR, OLD, NEW)     \
    (__sync_bool_compare_and_swap(PTR, OLD, NEW))

#define NOTIFY_MSG "!"
#define NOTIFY_MSG_LEN 1

/* Spin attempting to atomically adjust F by ADJ until successful */
#define SPIN_ADJ(F, ADJ)                                                \
    do {                                                                \
        for (;;) {                                                      \
            size_t v = F;                                               \
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&F, v, v + ADJ)) {         \
                break;                                                  \
            }                                                           \
        }                                                               \
    } while (0)


static void notify_new_task(struct threadpool *t);
static bool notify_shutdown(struct threadpool *t);
static bool spawn(struct threadpool *t);
static void *thread_task(void *thread_info);

#define DEFAULT_MAX_DELAY 100   /* msec */
#define DEFAULT_TASK_RINGBUF_SIZE2 8
#define DEFAULT_MAX_THREADS 4

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
    struct threadpool_task *tasks = NULL;
    struct thread_info *threads = NULL;

    t = malloc(sizeof(*t));
    if (t == NULL) { goto cleanup; }

    size_t tasks_sz = (1 << cfg->task_ringbuf_size2) * sizeof(struct threadpool_task);
    size_t threads_sz = cfg->max_threads * sizeof(struct thread_info);

    tasks = malloc(tasks_sz);
    if (tasks == NULL) { goto cleanup; }

    threads = malloc(threads_sz);
    if (threads == NULL) { goto cleanup; }

    memset(t, 0, sizeof(*t));
    memset(tasks, 0, tasks_sz);
    memset(threads, 0, threads_sz);

    t->tasks = tasks;
    t->threads = threads;
    t->task_ringbuf_size2 = cfg->task_ringbuf_size2;
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

    size_t queue_size = (1 << t->task_ringbuf_size2);
    size_t mask = queue_size - 1;

    for (;;) {
        size_t wh = t->task_reserve_head;
        size_t rh = t->task_release_head;

        size_t rem = ((wh + queue_size) - rh) & mask;
        if (rem == mask) {
            if (pushback) { *pushback = rem; }
            return false;       /* full, cannot schedule */
        }
        struct threadpool_task *tbuf = &t->tasks[wh & mask];

        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_reserve_head, wh, wh + 1)) {
            memcpy(tbuf, task, sizeof(*task));

            notify_new_task(t);
            if (pushback) { *pushback = rem; }
            SPIN_ADJ(t->task_commit_head, 1);
            SPIN_ADJ(t->tasks_assigned, 1);
            return true;
        }
    }
}

void threadpool_stats(struct threadpool *t, struct threadpool_info *ti) {
    if (ti) {
        ti->active_threads = t->active_threads;
        ti->dormant_threads = t->live_threads - t->active_threads;
        ti->tasks_assigned = t->tasks_assigned;
        ti->tasks_completed = t->tasks_completed;
        ti->backlog_size = t->task_commit_head - t->task_request_head;
    }
}

bool threadpool_shutdown(struct threadpool *t, bool kill_all) {
    size_t mask = (1 << t->task_ringbuf_size2) - 1;

    while (t->task_commit_head > t->task_request_head) {
        size_t rh = t->task_request_head;

        struct threadpool_task *tbuf = &t->tasks[rh & mask];
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_request_head, rh, rh + 1)) {
            if (tbuf->cleanup) {
                tbuf->cleanup(tbuf->udata);
                memset(tbuf, 0, sizeof(*tbuf));
            }
            SPIN_ADJ(t->task_release_head, 1);
        }
    }

    notify_shutdown(t);
    if (kill_all) {
        /* TODO: pthread_kill threads and set STATUS_SHUTDOWN ... */
    }
    return true;
}

void threadpool_free(struct threadpool *t) {
    free(t->tasks);
    t->tasks = NULL;
    free(t->threads);
    t->threads = NULL;
    free(t);
}

static void notify_new_task(struct threadpool *t) {
    /* FIXME: should this be 'if any are sleeping'? needs benchmarking. */
    if (t->active_threads < t->live_threads) { /* wake up */
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
    }

    if (t->live_threads < t->max_threads) { /* spawn */
        if (spawn(t)) {
            SPIN_ADJ(t->active_threads, 1);
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
        printf("PIPE failed!\n");
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

    size_t mask = (1 << t->task_ringbuf_size2) - 1;
    struct pollfd pfd[1] = { { .fd=ti->child_fd, .events=POLLIN }, };
    uint8_t read_buf[NOTIFY_MSG_LEN];

    size_t delay = 1;

    for (;;) {
        if (t->task_request_head == t->task_commit_head) {
            if (ti->status == STATUS_AWAKE) {
                ti->status = STATUS_ASLEEP;
                SPIN_ADJ(t->active_threads, -1);
            } else {
                delay <<= 1;
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
                    ti->status = STATUS_AWAKE;
                    delay = 1;
                    SPIN_ADJ(t->active_threads, 1);
                    ssize_t rres = read(ti->child_fd, read_buf, NOTIFY_MSG_LEN);
                    if (rres < 0) {
                        assert(0);
                    }
                }
            }
        }

        for (;;) {
            size_t wh = t->task_commit_head;
            size_t rh = t->task_request_head;
            if (rh == wh) {
                break;          /* nothing to do */
            }
            struct threadpool_task *task = &t->tasks[rh & mask];
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&t->task_request_head, rh, rh + 1)) {
                task->task(task->udata);
                SPIN_ADJ(t->task_release_head, 1);
                SPIN_ADJ(t->tasks_completed, 1);
                break;
            }
        }
    }
}
