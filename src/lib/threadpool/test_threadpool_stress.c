/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#include "threadpool.h"

/* Stress maximum throughput of no-op tasks */

static size_t task_count = 0;
static size_t last_count = 0;

static void dump_stats(const char *prefix, struct threadpool_info *stats, size_t ticks) {
    size_t delta = task_count - last_count;
    printf("%s  -- %8ld thread tasks / sec -- (at %d, dt %d, bl %zd) -- delta %zd\n",
        prefix, task_count / ticks,
        stats->active_threads, stats->dormant_threads,
        stats->backlog_size, delta);
    last_count = task_count;
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

static size_t fibs(size_t arg) {
    if (arg < 2) { return 1; }
    return fibs(arg - 1) + fibs(arg - 2);
}

static void task_cb(void *udata) {
    SPIN_ADJ(task_count, 1);
    (void)fibs;
    (void)udata;
    (void)task_count;
}

int main(int argc, char **argv) {
    uint8_t sz2 = 12;
    uint8_t max_threads = 8;

    char *sz2_env = getenv("SZ2");
    char *max_threads_env = getenv("MAX_THREADS");
    if (sz2_env) { sz2 = atoi(sz2_env); }
    if (max_threads_env) { max_threads = atoi(max_threads_env); }

    struct threadpool_config cfg = {
        .task_ringbuf_size2 = sz2,
        .max_threads = max_threads,
    };
    struct threadpool *t = threadpool_init(&cfg);
    assert(t);

    struct threadpool_task task = {
        .task = task_cb, .udata = t,
    };

    struct threadpool_info stats;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t last_sec = tv.tv_sec;
    size_t counterpressure = 0;
    size_t ticks = 0;

    for (;;) {
        gettimeofday(&tv, NULL);
        if (tv.tv_sec > last_sec) {
            last_sec = tv.tv_sec;
            threadpool_stats(t, &stats);
            ticks++;
            dump_stats("tick...", &stats, ticks);
        }

        for (size_t i = 0; i < 1000; i++) {
            if (!threadpool_schedule(t, &task, &counterpressure)) {
                size_t msec = i * 1000 * counterpressure;
                usleep(msec >> 12);
            } else {
                break;
            }
        }
    }

    return 0;
}
