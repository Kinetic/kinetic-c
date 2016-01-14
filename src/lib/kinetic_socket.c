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

#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "kinetic.pb-c.h"
#include "protobuf-c/protobuf-c.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include "socket99.h"

int KineticSocket_Connect(const char* host, int port)
{
    char port_str[32];
    struct addrinfo hints;
    struct addrinfo* ai_result = NULL;
    struct addrinfo* ai = NULL;
    socket99_result result;

    // Setup server address info
    socket99_config cfg = {
        .host = (char*)host,
        .port = port,
        .nonblocking = true,
    };
    sprintf(port_str, "%d", port);

    // Open socket
    LOGF1("Connecting to %s:%d", host, port);
    if (!socket99_open(&cfg, &result)) {
        char err_buf[256];
        socket99_snprintf(err_buf, 256, &result);
        LOGF0("Failed to open socket connection with host: %s", err_buf);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }

    // Configure the socket
    socket99_set_hints(&cfg, &hints);
    if (getaddrinfo(cfg.host, port_str, &hints, &ai_result) != 0) {
        LOGF0("Failed to get socket address info: errno %d", errno);
        close(result.fd);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }

    KineticSocket_EnableTCPNoDelay(result.fd);

    for (ai = ai_result; ai != NULL; ai = ai->ai_next) {
        int setsockopt_result;
        int buffer_size = KINETIC_OBJ_SIZE;

#if defined(SO_NOSIGPIPE) && !defined(__APPLE__)
        // On BSD-like systems we can set SO_NOSIGPIPE on the socket to
        // prevent it from sending a PIPE signal and bringing down the whole
        // application if the server closes the socket forcibly
        int enable = 1;
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_NOSIGPIPE,
                                       &enable, sizeof(enable));
        // Allow ENOTSOCK because it allows tests to use pipes instead of
        // real sockets
        if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK) {
            LOG0("Failed to set SO_NOSIGPIPE on socket");
            continue;
        }
#endif

        // Increase send buffer to KINETIC_OBJ_SIZE
        // Note: OS allocates 2x this value for its overhead
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_SNDBUF,
                                       &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1) {
            LOG0("Error setting socket send buffer size");
            continue;
        }

        // Increase receive buffer to KINETIC_OBJ_SIZE
        // Note: OS allocates 2x this value for its overheadbuffer_size
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_RCVBUF,
                                       &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1) {
            LOG0("Error setting socket receive buffer size");
            continue;
        }

        break;
    }

    freeaddrinfo(ai_result);

    if (ai == NULL || result.fd == KINETIC_SOCKET_DESCRIPTOR_INVALID) {
        // we went through all addresses without finding one we could bind to
        LOGF0("Could not connect to %s:%d", host, port);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }
    else {
        LOGF1("Successfully connected to %s:%d (fd=%d)", host, port, result.fd);
        return result.fd;
    }
}

void KineticSocket_Close(int fd)
{
    if (fd == -1) {
        LOG1("Not connected so no cleanup needed");
    }
    else {
        LOGF0("Closing socket with fd=%d", fd);
        if (close(fd) == 0) {
            LOG2("Socket closed successfully");
        }
        else {
            LOGF0("Error closing socket file descriptor!"
                 " (fd=%d, errno=%d, desc='%s')",
                 fd, errno, strerror(errno));
        }
    }
}

void KineticSocket_EnableTCPNoDelay(int fd)
{
    int on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

void KineticSocket_BeginPacket(int fd)
{
#if !defined(__APPLE__) /* TCP_CORK is NOT available on OSX */
    int on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));
#else
    (void)fd;
#endif
}

void KineticSocket_FinishPacket(int fd)
{
#if !defined(__APPLE__) /* TCP_CORK is NOT available on OSX */
    int off = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
#else
    (void)fd;
#endif
}
