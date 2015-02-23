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
#include <getopt.h>

static char *kinetic_host = "localhost";
static long idle_seconds = 0;

static void usage(void) {
    fprintf(stderr, "usage: setup_teardown [-h KINETIC_HOST] [-i IDLE_SECONDS]\n");
    exit(1);
}

static void handle_args(int argc, char **argv) {
    int fl = 0;
    while ((fl = getopt(argc, argv, "i:h:")) != -1) {
        switch (fl) {
        case 'h':               /* host */
            kinetic_host = optarg;
            break;
        case 'i':               /* idle seconds */
            idle_seconds = strtol(optarg, NULL, 10);
            break;
        case '?':
        default:
            usage();
        }
    }
}

int main(int argc, char** argv)
{
    handle_args(argc, argv);

    // Initialize kinetic-c and configure sessions
    KineticSession* session;
    KineticClientConfig clientConfig = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&clientConfig);
    if (client == NULL) { return 1; }
    const char HmacKeyString[] = "asdfasdf";
    KineticSessionConfig sessionConfig = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = ByteArray_CreateWithCString(HmacKeyString),
    };
    KineticStatus status = KineticClient_CreateSession(&sessionConfig, client, &session);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to the Kinetic device w/status: %s\n",
            Kinetic_GetStatusDescription(status));
        exit(1);
    }

    if (idle_seconds > 0) {
        printf(" -- Sleeping %ld seconds\n", idle_seconds);
        sleep(idle_seconds);
    }

    // Shutdown client connection and cleanup
    KineticClient_DestroySession(session);
    KineticClient_Shutdown(client);
    return 0;
}
