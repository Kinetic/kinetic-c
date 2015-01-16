/*
* kinetic-c
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "byte_array.h"
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    char *kinetic_host = getenv("KINETIC_HOST");
    if (kinetic_host == NULL) {
        kinetic_host = "localhost";
    }

    // Establish connection
    KineticStatus status;
    const char HmacKeyString[] = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .port = KINETIC_PORT,
            .clusterVersion = 0,
            .identity = 1,
            .hmacKey = ByteArray_CreateWithCString(HmacKeyString)
        }
    };

    strncpy(session.config.host, kinetic_host, HOST_NAME_MAX - 1);
    session.config.host[HOST_NAME_MAX - 1] = '\0';

    KineticClient * client = KineticClient_Init("stdout", 1);
    if (client == NULL) { return 1; }
    status = KineticClient_CreateConnection(&session, client);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Connection to host '%s' failed w/ status: %s\n",
            session.config.host, Kinetic_GetStatusDescription(status));
        exit(1);
    }

    char *idle_seconds_var = getenv("IDLE_SECONDS");
    long idle_seconds = 0;
    if (idle_seconds_var) {
        strtol(idle_seconds_var, NULL, 10);
    }

    if (idle_seconds > 0) {
        printf(" -- Sleeping %ld seconds\n", idle_seconds);
        sleep(idle_seconds);
    }

    // Shutdown client connection and cleanup
    KineticClient_DestroyConnection(&session);
    KineticClient_Shutdown(client);
    return 0;
}
