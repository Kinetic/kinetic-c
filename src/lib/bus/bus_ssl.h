#ifndef BUS_SSL_H
#define BUS_SSL_H

#include "bus.h"
#include "bus_internal_types.h"

/* Default to TLS 1.1 for now, since it's what the drives support. */
#define KINETIC_USE_TLS_1_2 0

/* Initialize the SSL library internals for use by the messaging bus. */
bool bus_ssl_init(struct bus *b);

/* Do an SSL / TLS shake for a connection. Blocking.
 * Returns whether the connection succeeded. */
bool bus_ssl_connect(struct bus *b, connection_info *ci);

void bus_ssl_free(struct bus *b);

#endif
