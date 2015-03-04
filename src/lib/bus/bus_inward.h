#ifndef BUS_INWARD_H
#define BUS_INWARD_H

#include "bus_types.h"

/* Get the string key for a log event ID. */
const char *bus_log_event_str(log_event_t event);

/* For a given file descriptor, get the listener ID to use.
 * This will level sockets between multiple threads. */
struct listener *bus_get_listener_for_socket(struct bus *b, int fd);

/* Schedule a task in the bus's threadpool. */
bool bus_schedule_threadpool_task(struct bus *b, struct threadpool_task *task,
    size_t *backpressure);

/* Lock / unlock the log mutex, since logging can occur on several threads. */
void bus_lock_log(struct bus *b);
void bus_unlock_log(struct bus *b);

/* Deliver a boxed message to the thread pool to execute. */
bool bus_process_boxed_message(struct bus *b,
    struct boxed_msg *box, size_t *backpressure);

/* Provide backpressure by sleeping for (backpressure >> shift) msec, if
 * the value is greater than 0. */
void bus_backpressure_delay(struct bus *b, size_t backpressure, uint8_t shift);

#endif
