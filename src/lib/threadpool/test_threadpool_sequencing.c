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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <poll.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>

#include "threadpool.h"

static size_t completed_count = 0;
#define MAX_TASKS 32

static void dump_stats(const char *prefix, struct threadpool_info *stats, size_t ticks) {
    printf("%s  -- %8ld thread tasks / sec -- (at %d, dt %d, ta %zd, tc %zd, bl %zd) -- %zd\n",
        prefix, stats->tasks_completed / ticks,
        stats->active_threads, stats->dormant_threads,
        stats->tasks_assigned, stats->tasks_completed, stats->backlog_size,
        completed_count);
}

#define ATOMIC_BOOL_COMPARE_AND_SWAP(PTR, OLD, NEW)     \
    (__sync_bool_compare_and_swap(PTR, OLD, NEW))

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

typedef struct {
    struct threadpool *t;
    struct threadpool_task *task;
    size_t count;
} env;

static size_t limit = 10000000;

static void task_cb(void *udata) {
    env *e = (env *)udata;
    if (e->count == limit) {
        SPIN_ADJ(completed_count, 1);
    } else {
        e->count++;
        if ((e->count & ((1 << 16) - 1)) == 0) {
            printf("count: %zd on %p\n", e->count, (void *)pthread_self());
        }
        for (;;) {
            if (threadpool_schedule(e->t, e->task, NULL)) {
                break;
            }
            usleep(1 * 1000);
        }
    }
}

int main(int argc, char **argv) {
    uint8_t sz2 = 8;
    uint8_t max_threads = 8;

    char *sz2_env = getenv("SZ2");
    char *max_threads_env = getenv("MAX_THREADS");
    char *limit_env = getenv("LIMIT");
    if (sz2_env) { sz2 = atoi(sz2_env); }
    if (max_threads_env) { max_threads = atoi(max_threads_env); }
    if (limit_env) { limit = atol(limit_env); }

    if (max_threads > MAX_TASKS) {
        printf("too many threads\n");
        exit(1);
    }
    
    struct threadpool_config cfg = {
        .task_ringbuf_size2 = sz2,
        .max_threads = max_threads,
    };
    struct threadpool *t = threadpool_init(&cfg);
    assert(t);

    struct threadpool_info stats;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t last_sec = tv.tv_sec;
    size_t counterpressure = 0;
    size_t ticks = 0;

    struct threadpool_task tasks[MAX_TASKS];
    env envs[MAX_TASKS];
    memset(&tasks, 0, sizeof(tasks));
    
    for (int i = 0; i < max_threads; i++) {
        envs[i] = (env){ .t = t, .task = &tasks[i], .count = 0, };
        tasks[i] = (struct threadpool_task){ .task = task_cb, .udata = &envs[i], };

        for (;;) {
            if (threadpool_schedule(t, &tasks[i], &counterpressure)) {
                break;
            }
        }        
    }

    printf("waiting...\n");

    while (completed_count < max_threads) {
        gettimeofday(&tv, NULL);
        if (tv.tv_sec > last_sec) {
            last_sec = tv.tv_sec;
            threadpool_stats(t, &stats);
            ticks++;
            dump_stats("tick...", &stats, ticks);
        }
    }

    return 0;
}
