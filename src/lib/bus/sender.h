#ifndef SENDER_H
#define SENDER_H

#include "bus_types.h"
#include "bus_internal_types.h"

/* Manager of active outgoing messages. */
struct sender;

struct sender *sender_init(struct bus *b, struct bus_config *cfg);

/* Send an outgoing message.
 * 
 * This blocks until the message has either been sent over a outgoing
 * socket or delivery has failed, to provide counterpressure. */
bool sender_enqueue_message(struct sender *s,
    boxed_msg *msg, int *response_fd);

bool sender_shutdown(struct sender *s);

void sender_free(struct sender *s);


#endif
