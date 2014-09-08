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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

#include "kinetic_client.h"
#include "noop.h"
#include "put.h"
#include "get.h"

typedef struct _Arguments {
    char host[HOST_NAME_MAX];
    int port;
    int nonBlocking;
    int useTls;
    int64_t clusterVersion;
    int64_t identity;
    char hmacKey[KINETIC_MAX_KEY_LEN];
    uint8_t value[PDU_VALUE_MAX_LEN];
    int64_t valueLength;
    char valueKey[KINETIC_MAX_KEY_LEN];
    char version[32];
    char newVersion[32];
    KineticProto_Algorithm algorithm;
} Arguments;

int main(int argc, char** argv)
{
    int status = -1;
    int opt;
    int optIndex = 0;

    // Create an ArgP processor to parse arguments
    Arguments cfg = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .nonBlocking = false,
        .useTls = false,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = "asdfasdf",
        .value = {0},
        .valueLength = 0,
        .valueKey = "some_value_key...",
        .version = "v1.0",
        .newVersion = "v1.0",
    };
    ByteArray hmacKey = BYTE_ARRAY_INIT_FROM_CSTRING(cfg.hmacKey);
    ByteArray value = {.data = cfg.value, .len = cfg.valueLength};

    struct option long_options[] =
    {
        {"non-blocking", no_argument,      &cfg.nonBlocking, true},
        {"blocking",    no_argument,       &cfg.nonBlocking, false},
        {"tls",         no_argument,       &cfg.port,        KINETIC_TLS_PORT},
        {"host",        required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "h", long_options, &optIndex)) != -1)
    {
        // Parse options until we reach the end of the argument list
        if (opt != -1)
        {
            switch (opt)
            {
                case 0:
                    // If this option set a flag, do nothing else now.
                    if (long_options[optIndex].flag != 0)
                        break;
                case 'h':
                    strcpy(cfg.host, optarg);
                    break;
                case '?':
                    // getopt_long already printed an error message.
                    break;
                default:
                    abort ();
            }
        }
    }

    // Execute all specified operations in order
    for (;optind < argc; optind++)
    {
        char* op = argv[optind];

        if (strcmp("noop", op) == 0)
        {
            printf("\n"
                   "Executing NoOp w/configuration:\n"
                   "-------------------------------\n"
                   "  host: %s\n"
                   "  port: %d\n"
                   "  non-blocking: %s\n"
                   "  clusterVersion: %lld\n"
                   "  identity: %lld\n"
                   "  key: '%s'\n",
                cfg.host,
                cfg.port,
                cfg.nonBlocking ? "true" : "false",
                (long long int)cfg.clusterVersion,
                (long long int)cfg.identity,
                cfg.hmacKey);
            status = NoOp(cfg.host, cfg.port, cfg.nonBlocking,
                cfg.clusterVersion, cfg.identity, hmacKey);
            if (status == 0)
            {
                printf("\nNoOp executed successfully!\n\n");
            }
            else
            {
                printf("\nNoOp operation failed! status=%d\n\n", status);
                return -1;
            }
        }

        else if (strcmp("put", op) == 0)
        {
            unsigned int i;
            for (i = 0; i < sizeof(cfg.value); i++)
            {
                cfg.value[i] = (uint8_t)(0x0ff & i);
            }

            Kinetic_KeyValue metadata = {
                .key = BYTE_ARRAY_INIT_FROM_CSTRING("some_value_key..."),
                .algorithm = KINETIC_PROTO_ALGORITHM_SHA1,
                .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
                .dbVersion = BYTE_ARRAY_NONE,
                .tag = BYTE_ARRAY_INIT_FROM_CSTRING("some_value_tag..."),
            };

            printf("\n"
                   "Executing Put w/configuration:\n"
                   "-------------------------------\n"
                   "  host: %s\n"
                   "  port: %d\n"
                   "  non-blocking: %s\n"
                   "  clusterVersion: %lld\n"
                   "  identity: %lld\n"
                   "  key: '%s'\n"
                   "  value: %zd bytes\n",
                cfg.host,
                cfg.port,
                cfg.nonBlocking ? "true" : "false",
                (long long int)cfg.clusterVersion,
                (long long int)cfg.identity,
                metadata.key.data,
                value.len);

            status = Put(
                cfg.host, cfg.port, cfg.nonBlocking,
                cfg.clusterVersion, cfg.identity, hmacKey,
                &metadata, value);

            if (status == 0)
            {
                printf("\nPut executed successfully!\n\n");
            }
            else
            {
                printf("\nPut operation failed! status=%d\n\n", status);
                return -1;
            }
        }

        else if (strcmp("get", op) == 0)
        {
            Kinetic_KeyValue metadata = {
                .key = BYTE_ARRAY_INIT_FROM_CSTRING("some_value_key..."),
                .algorithm = KINETIC_PROTO_ALGORITHM_SHA1,
                .newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0"),
                .dbVersion = BYTE_ARRAY_NONE,
                .tag = BYTE_ARRAY_INIT_FROM_CSTRING("some_value_tag..."),
            };

            printf("\n"
                   "Executing Get w/configuration:\n"
                   "-------------------------------\n"
                   "  host: %s\n"
                   "  port: %d\n"
                   "  non-blocking: %s\n"
                   "  clusterVersion: %lld\n"
                   "  identity: %lld\n"
                   "  hmacKey: '%s'\n"
                   "  value: %zd bytes\n",
                cfg.host,
                cfg.port,
                cfg.nonBlocking ? "true" : "false",
                (long long int)cfg.clusterVersion,
                (long long int)cfg.identity,
                (char*)hmacKey.data,
                value.len);

            status = Get(cfg.host, cfg.port, cfg.nonBlocking,
                cfg.clusterVersion, cfg.identity, hmacKey,
                &metadata, value);

            if (status == 0 || status == KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR)
            {
                printf("\nGet executed successfully!\n\n");
                return 0;
            }
            else
            {
                printf("\nGet operation failed! status=%d\n\n", status);
                return -1;
            }
        }

        else
        {
            printf("\nSpecified operation is invalid!\n");
            return -1;
        }

    }

    return status;
}
