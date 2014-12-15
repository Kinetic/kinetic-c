#ifndef BUS_H
#define BUS_H

#include "bus_types.h"

/* This opaque bus struct represents the only user-facing interface to
 * the network handling code. Callbacks are provided to react to network
 * events. */

/* Initialize a bus, based on configuration in *config. Returns a bool
 * indicating whether the construction succeeded, and the bus pointer
 * and/or a status code indicating the cause of failure in *res. */
bool bus_init(bus_config *config, struct bus_result *res);

/* Send a request. Blocks until the request has been transmitted.
 *
 * TODO: liveness of msg: copy or take ownership?
 * 
 * Assumes the FD has been registered with bus_register_socket;
 * sending to an unregistered socket is an error. */
bool bus_send_request(struct bus *b, bus_user_msg *msg);

/* Get the string key for a log event ID. */
const char *bus_log_event_str(log_event_t event);


/* Register a socket connected to an endpoint, and data that will be passed
 * to all interactions on that socket. 
 * 
 * The socket will have request -> response messages with timeouts, as
 * well as unsolicited status messages. */
bool bus_register_socket(struct bus *b, int fd, void *socket_udata);

/* Begin shutting the system down. Returns true once everything pending
 * has resolved. */
bool bus_shutdown(struct bus *b);

/* For a given file descriptor, get the listener / sender ID to use.
 * This will level sockets between multiple threads. */
struct sender *bus_get_sender_for_socket(struct bus *b, int fd);
struct listener *bus_get_listener_for_socket(struct bus *b, int fd);

/* Schedule a task in the bus's threadpool. */
bool bus_schedule_threadpool_task(struct bus *b, struct threadpool_task *task,
    size_t *backpressure);

/* Lock / unlock the log mutex, since logging can occur on several threads. */
void bus_lock_log(struct bus *b);
void bus_unlock_log(struct bus *b);

/* Free internal data structures for the bus. */
void bus_free(struct bus *b);

/* Deliver a boxed message to the thread pool to execute. */
bool bus_process_boxed_message(struct bus *b,
    struct boxed_msg *box, size_t *backpressure);

#endif
