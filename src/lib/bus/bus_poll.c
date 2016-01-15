/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
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

bool BusPoll_OnCompletion(struct bus *b, int fd) {
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
            if (Util_IsResumableIOError(errno)) {
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
                Bus_BackpressureDelay(b, msec, LISTENER_BACKPRESSURE_SHIFT);
                BUS_LOG(b, 4, LOG_SENDING_REQUEST, "sent!", b->udata);
                return true;
            } else if (sz == -1) {
                if (Util_IsResumableIOError(errno)) {
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
            /* This should never happen, but I have seen it occur on OSX.
             * If we log it, reset errno, and continue, it does not appear
             * to fall into busywaiting by always returning 0. */
            BUS_LOG_SNPRINTF(b, 1, LOG_SENDING_REQUEST, b->udata, 64,
                "poll_on_completion, blocking forever returned %d, errno %d", res, errno);
            errno = 0;
        }
    }
}
