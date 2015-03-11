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
#include "bus_poll.h"
#include "syscall.h"
#include "listener.h"
#include "util.h"

#include <assert.h>
#include <errno.h>

#ifdef TEST
struct pollfd fds[1];
uint8_t read_buf[sizeof(uint8_t) + sizeof(uint16_t)];
int poll_errno;
int read_errno;
#endif

bool bus_poll_on_completion(struct bus *b, int fd) {
    /* POLL in a pipe */
    #ifndef TEST
    struct pollfd fds[1];
    #endif
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    for (;;) {
        BUS_LOG(b, 5, LOG_SENDING_REQUEST, "Polling on completion...tick...", b->udata);
        #ifdef TEST
        errno = poll_errno;
        #endif
        BUS_LOG_SNPRINTF(b, 5, LOG_SENDING_REQUEST, b->udata, 64,
            "poll_on_completion, polling %d", fd);
        int res = syscall_poll(fds, 1, -1);
        BUS_LOG_SNPRINTF(b, 5, LOG_SENDING_REQUEST, b->udata, 64,
            "poll_on_completion for %d, res %d (errno %d)", fd, res, errno);
        if (res == -1) {
            if (util_is_resumable_io_error(errno)) {
                BUS_LOG_SNPRINTF(b, 5, LOG_SENDING_REQUEST, b->udata, 64,
                    "poll_on_completion, resumable IO error %d", errno);
                errno = 0;
                continue;
            } else {
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDING_REQUEST, b->udata, 64,
                    "poll_on_completion, non-resumable IO error %d", errno);
                return false;
            }
        } else if (res == 1) {
            uint16_t msec = 0;
            #ifndef TEST
            uint8_t read_buf[sizeof(uint8_t) + sizeof(uint16_t)];
            #endif

            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                BUS_LOG(b, 1, LOG_SENDING_REQUEST, "failed (broken alert pipe)", b->udata);
                return false;
            }

            BUS_LOG(b, 3, LOG_SENDING_REQUEST, "Reading alert pipe...", b->udata);
            #ifdef TEST
            errno = read_errno;
            #endif
            ssize_t sz = syscall_read(fd, read_buf, sizeof(read_buf));

            if (sz == sizeof(read_buf)) {
                /* Payload: little-endian uint16_t, msec of backpressure. */
                assert(read_buf[0] == LISTENER_MSG_TAG);

                msec = (read_buf[1] << 0) + (read_buf[2] << 8);
                bus_backpressure_delay(b, msec, LISTENER_BACKPRESSURE_SHIFT);
                BUS_LOG(b, 4, LOG_SENDING_REQUEST, "sent!", b->udata);
                return true;
            } else if (sz == -1) {
                if (util_is_resumable_io_error(errno)) {
                    BUS_LOG_SNPRINTF(b, 5, LOG_SENDING_REQUEST, b->udata, 64,
                        "poll_on_completion read, resumable IO error %d", errno);
                    errno = 0;
                    continue;
                } else {
                    BUS_LOG_SNPRINTF(b, 2, LOG_SENDING_REQUEST, b->udata, 64,
                        "poll_on_completion read, non-resumable IO error %d", errno);
                    errno = 0;
                    return false;
                }
            } else {
                BUS_LOG_SNPRINTF(b, 1, LOG_SENDING_REQUEST, b->udata, 64,
                    "poll_on_completion bad read size %zd", sz);
                return false;
            }
        } else {
            BUS_LOG_SNPRINTF(b, 0, LOG_SENDING_REQUEST, b->udata, 64,
                "poll_on_completion, blocking forever returned 0, errno %d", errno);
            assert(false);
        }
    }
}
