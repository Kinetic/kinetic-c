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
#include <ctype.h>

#include <openssl/sha.h>

static void do_noop(KineticSession *session) {
    /* Send a NoOp command. */
    KineticStatus status = KineticClient_NoOp(session);
    printf("NoOp status: %s\n", Kinetic_GetStatusDescription(status));

    /* No cleanup necessary */
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Establish connection
    KineticStatus status;
    const char HmacKeyString[] = "asdfasdf";
    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = "localhost",
            .port = KINETIC_PORT,
            .clusterVersion = 0,
            .identity = 1,
            .hmacKey = ByteArray_CreateWithCString(HmacKeyString)
        }
    };
    
    KineticClientConfig client_config = {
        .logFile = "stdout",
        .logLevel = 1,
    };
    KineticClient * client = KineticClient_Init(&client_config);
    if (client == NULL) { return 1; }
    status = KineticClient_CreateConnection(&session, client);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Connection to host '%s' failed w/ status: %s\n",
            session.config.host, Kinetic_GetStatusDescription(status));
        exit(1);
    }

    do_noop(&session);
    
    // Shutdown client connection and cleanup
    KineticClient_DestroyConnection(&session);
    KineticClient_Shutdown(client);
    return 0;
}
