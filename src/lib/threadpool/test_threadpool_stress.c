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
    struct threadpool *t = Threadpool_Init(&cfg);
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
            Threadpool_Stats(t, &stats);
            ticks++;
            dump_stats("tick...", &stats, ticks);
        }

        /* Every 16 seconds, pause scheduling for 5 seconds to test
         * thread sleep/wake-up alerts. */
        if ((ticks & 15) == 0) {
            sleep(5);
        }

        for (size_t i = 0; i < 1000; i++) {
            if (!Threadpool_Schedule(t, &task, &counterpressure)) {
                size_t msec = i * 1000 * counterpressure;
                usleep(msec >> 12);
            } else {
                break;
            }
        }
    }

    return 0;
}
