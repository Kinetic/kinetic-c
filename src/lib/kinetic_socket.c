/*
 * kinetic-c-client
 * Copyright (C) 2014 Seagate Technology.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "kinetic_socket.h"
#include "kinetic_logger.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int KineticSocket_Connect(const char* host, int port, bool blocking)
{
    char message[256];
    char port_str[32];
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    struct addrinfo* ai = NULL;
    int socket_fd;
    int res;

    sprintf(message, "Connecting to %s:%d", host, port);
    LOG(message);

    memset(&hints, 0, sizeof(struct addrinfo));

    // could be inet or inet6
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;

    sprintf(port_str, "%d", port);

    res = getaddrinfo(host, port_str, &hints, &result);
    if (res != 0)
    {
        LOG("Could not resolve host");
        return -1;
    }

    for (ai = result; ai != NULL; ai = ai->ai_next)
    {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        int res;

        res = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host), service,
                sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
        if (res != 0)
        {
            LOG("Could not get name info");
            continue;
        }
        else
        {
            LOG("Trying to connect to host");
        }

        socket_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (socket_fd == -1)
        {
            LOG("Could not create socket");
            continue;
        }

        // os x won't let us set close-on-exec when creating the socket, so set it separately
        int current_fd_flags = fcntl(socket_fd, F_GETFD);
        if (current_fd_flags == -1)
        {
            LOG("Failed to get socket fd flags");
            close(socket_fd);
            continue;
        }
        if (fcntl(socket_fd, F_SETFD, current_fd_flags | FD_CLOEXEC) == -1)
        {
            LOG("Failed to set socket close-on-exit");
            close(socket_fd);
            continue;
        }

        // On BSD-like systems we can set SO_NOSIGPIPE on the socket to prevent it from sending a
        // PIPE signal and bringing down the whole application if the server closes the socket
        // forcibly
// #ifdef SO_NOSIGPIPE
//         int set = 1;
//         int setsockopt_result = setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
//         // Allow ENOTSOCK because it allows tests to use pipes instead of real sockets
//         if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK)
//         {
//             LOG("Failed to set SO_NOSIGPIPE on socket");
//             close(socket_fd);
//             continue;
//         }
// #endif

        if (connect(socket_fd, ai->ai_addr, ai->ai_addrlen) == -1)
        {
            LOG("Unable to connect");
            close(socket_fd);
            continue;
        }
        else
        {
            LOG("Connected to host!");
        }

        if (!blocking && fcntl(socket_fd, F_SETFL, O_NONBLOCK) != 0)
        {
            LOG("Failed to set socket nonblocking");
            close(socket_fd);
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (ai == NULL)
    {
        // we went through all addresses without finding one we could bind to
        sprintf(message, "Could not connect to %s:%d", host, port);
        LOG(message);
        return -1;
    }

    return socket_fd;
}

void KineticSocket_Close(int socket_descriptor)
{
    char message[64];
    if (socket_descriptor == -1)
    {
        LOG("Not connected so no cleanup needed");
    }
    else
    {
        sprintf(message, "Closing socket with fd=%d", socket_descriptor);
        LOG(message);
        if (close(socket_descriptor))
        {
            LOG("Error closing socket file descriptor");
        }
    }
}

