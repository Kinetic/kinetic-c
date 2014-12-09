#ifndef LISTENER_H
#define LISTENER_H

#include "bus_types.h"
#include "bus_internal_types.h"
#include "casq.h"

/* Manager of incoming messages from drives, both responses and
 * unsolicited status updates. */
struct listener;

struct listener *listener_init(struct bus *b, struct bus_config *cfg);

/* Add/remove sockets' metadata from internal info. */
bool listener_add_socket(struct listener *l, connection_info *ci, int notify_fd);
bool listener_close_socket(struct listener *l, int fd);

bool listener_expect_response(struct listener *l, boxed_msg *box,
    uint16_t *backpressure);

bool listener_shutdown(struct listener *l);

void listener_free(struct listener *l);

#endif
