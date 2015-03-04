#ifndef BUS_POLL_H
#define BUS_POLL_H

#include <stdbool.h>
#include "bus_types.h"

/* Poll on fd until complete, return true on success or false on IO
 * error. (This is mostly in a distinct module to add a testing seam.) */
bool bus_poll_on_completion(struct bus *b, int fd);

#endif
