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

#include "threadpool.h"

static size_t task_count = 0;

static size_t fibs(size_t arg) {
    if (arg < 2) { return 1; }
    return fibs(arg - 1) + fibs(arg - 2);
}

static void dump_stats(const char *prefix, struct threadpool_info *stats) {
    printf("%s(at %d, dt %d, bl %zd)\n",
        prefix, stats->active_threads, stats->dormant_threads,
        stats->backlog_size);
}

#define ATOMIC_BOOL_COMPARE_AND_SWAP(PTR, OLD, NEW)     \
    (__sync_bool_compare_and_swap(PTR, OLD, NEW))

static void task_cb(void *udata) {
    struct threadpool *t = (struct threadpool *)udata;

    for (;;) {
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&task_count, task_count, task_count + 1)) {
            break;
        }
    }

    size_t arg = task_count;
    arg = 30 + (random() % 10);

    size_t tc = task_count;

    size_t res = fibs(arg);

    printf("%zd -- fibs(%zd) => %zd", tc, arg, res);
    
    struct threadpool_info stats;
    Threadpool_Stats(t, &stats);
    dump_stats("", &stats);
}

static void inf_loop_cb(void *env) {
    (void)env;
    for (;;) {
        sleep(1);
    }
}

int main(int argc, char **argv) {
    uint8_t sz2 = 8;
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

    for (int i = 0; i < 4; i++) {
        printf("scheduling...\n");
        for (int j = 0; j < 40; j++) {
            for (;;) {
                size_t counterpressure = 0;
                if (Threadpool_Schedule(t, &task, &counterpressure)) {
                    break;
                } else {
                    size_t msec = 10 * 1000 * counterpressure;
                    usleep(msec);
                }
            }
        }

        Threadpool_Stats(t, &stats);
        dump_stats("sleeping...", &stats);
        sleep(1);
        Threadpool_Stats(t, &stats);
        dump_stats("waking...", &stats);
    }

    while (task_count < 4 * 40) {
        sleep(1);
    }

    task.task = inf_loop_cb;
    size_t counterpressure = 0;
    while (!Threadpool_Schedule(t, &task, &counterpressure)) {
        usleep(10 * 1000);
    }

    sleep(1);

    const int THREAD_SHUTDOWN_SECONDS = 5;
    printf("shutting down...\n");

    int limit = (1000 * THREAD_SHUTDOWN_SECONDS)/10;
    for (int i = 0; i < limit; i++) {
        if (Threadpool_Shutdown(t, false)) { break; }
        (void)poll(NULL, 0, 10);

        if (i == limit - 1) {
            printf("cancelling thread in intentional infinite loop\n");
            Threadpool_Shutdown(t, true);
        }
    }
    Threadpool_Free(t);

    return 0;
}
