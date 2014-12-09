#include <stdbool.h>
#include <errno.h>

#include "util.h"

bool util_is_resumable_io_error(int errno_) {
    return errno_ == EAGAIN || errno_ == EINTR || errno_ == EWOULDBLOCK;
}
