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
