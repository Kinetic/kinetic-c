#ifndef THREADPOOL_INTERNALS_H
#define THREADPOOL_INTERNALS_H

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

/* A task, with an additional mark. */
struct marked_task {
    threadpool_task_cb *task;
    threadpool_task_cleanup_cb *cleanup;
    void *udata;
    /* This mark is used to indicate which tasks can have the comit_head
     * and release_head counters advanced past them.
     * 
     * mark == (task_commit_head that points at tast): commit
     * mark == ~(task_release_head that points at tast): release
     *
     * Any other value means it is in an intermediate state and should be
     * retried later. */
    size_t mark;
};

/* Internal threadpool state */
struct threadpool {
    /* reserve -> commit -> request -> release */
    size_t task_reserve_head;   /* reserved for write */
    size_t task_commit_head;    /* done with write */
    size_t task_request_head;   /* requested for read */
    size_t task_release_head;   /* done processing task, can overwrite */

    struct marked_task *tasks;
    /* user stats */
    size_t task_ringbuf_size;
    size_t task_ringbuf_mask;

    size_t max_delay;           /* ceil of exponential sleep back-off */
    uint8_t task_ringbuf_size2;

    bool shutting_down;
    uint8_t live_threads;
    uint8_t max_threads;
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
static void commit_current_task(struct threadpool *t, struct marked_task *task, size_t wh);
static void release_current_task(struct threadpool *t, struct marked_task *task, size_t rh);

#endif
