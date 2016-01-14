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

#ifndef SYSCALL_H
#define SYSCALL_H

#include "bus_internal_types.h"
#include <poll.h>
#include <time.h>

/** Wrappers for syscalls, to allow mocking for testing. */
int syscall_poll(struct pollfd fds[], nfds_t nfds, int timeout);
int syscall_close(int fd);
ssize_t syscall_write(int fildes, const void *buf, size_t nbyte);
ssize_t syscall_read(int fildes, void *buf, size_t nbyte);

/** Wrappers for OpenSSL calls. */
int syscall_SSL_write(SSL *ssl, const void *buf, int num);
int syscall_SSL_read(SSL *ssl, void *buf, int num);
int syscall_SSL_get_error(const SSL *ssl, int ret);

/** Wrapper for gettimeofday and (where available)
 * clock_gettime(CLOCK_MONOTONIC), which is used when
 * RELATIVE is true. */
int syscall_timestamp(struct timeval *restrict tp, bool relative);

/** Wrapper for pthread calls. */
int syscall_pthread_join(pthread_t thread, void **value_ptr);

#endif
