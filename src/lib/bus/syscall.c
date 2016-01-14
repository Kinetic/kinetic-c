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

#include "syscall.h"

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

/* Wrappers for syscalls, to allow mocking for testing. */
int syscall_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    return poll(fds, nfds, timeout);
}

int syscall_close(int fd) {
    return close(fd);
}

ssize_t syscall_write(int fildes, const void *buf, size_t nbyte) {
    return write(fildes, buf, nbyte);
}

ssize_t syscall_read(int fildes, void *buf, size_t nbyte) {
    return read(fildes, buf, nbyte);
}

/* Wrappers for OpenSSL calls. */
int syscall_SSL_write(SSL *ssl, const void *buf, int num) {
    return SSL_write(ssl, buf, num);
}

int syscall_SSL_read(SSL *ssl, void *buf, int num) {
    return SSL_read(ssl, buf, num);
}

int syscall_SSL_get_error(const SSL *ssl, int ret) {
    return SSL_get_error(ssl, ret);
}


/* Wrapper for gettimeofday and (where available)
 * clock_gettime(CLOCK_MONOTONIC), which is used when
 * RELATIVE is true. */
int syscall_timestamp(struct timeval *restrict tv, bool relative) {
#if 0
    if (relative) {
        struct timespec ts;
        if (0 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
            return -1;
        }
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000L;
        return 0;
    }
#else
    (void)relative;
#endif
    return gettimeofday(tv, NULL);
}

int syscall_pthread_join(pthread_t thread, void **value_ptr)
{
    return pthread_join(thread, value_ptr);
}
