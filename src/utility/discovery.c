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

#include <stdio.h>

#include <err.h>
#include <errno.h>

#include <netinet/in.h>

#include "kinetic_client.h"
#include "socket99.h"
#include "json.h"

static int discover_service(void);


//------------------------------------------------------------------------------
// Main Entry Point Definition
int main(int argc, char** argv)
{
    // TODO: CLI args?
    (void)argc;
    (void)argv;
    return discover_service();
}
 
   
//------------------------------------------------------------------------------
// Service discovery

static int discover_service(void) {
    int v_true = 1;
    socket99_config cfg = {
        .host = INADDR_ANY,
        .port = KINETIC_PORT,
        .server = true,
        .datagram = true,
        .sockopts = {
            {/*SOL_SOCKET,*/ SO_BROADCAST, &v_true, sizeof(v_true)},
        },
    };
    socket99_result res;

    if (!socket99_open(&cfg, &res)) {
        errno = res.saved_errno;
        printf("res %d, %d\n", res.status, res.getaddrinfo_error);
        if (res.status == SOCKET99_ERROR_GETADDRINFO) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res.getaddrinfo_error));
            return 1;
        }
        err(1, "socket99_open");
        return 1;
    }

    int one = 1;
    if (0 != setsockopt(res.fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one))) {
        err(1, "setsockopt");
    }

    char buf[1024];
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);

    /* TODO:
     *     + nonblocking, with max timeout
     *     + set up multicast on 239.1.2.3
     *     + if we receive any info, print it */

    for (;;) {
        ssize_t received = recvfrom(res.fd, buf, sizeof(buf), 0,
            (struct sockaddr *)&client_addr, &addr_len);

        if (received > 0) {
            buf[received] = '\0';
            printf("Got: '%s'\n", buf);
            /* TODO: sink into json, print decoded data. */
        }
    }
    
    return 0;
}
