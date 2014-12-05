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
#include <stdio.h>
#include <getopt.h>

static KineticSession Session;
static uint8_t HmacData[1024];
static KineticEntry Entry;
static uint8_t KeyData[1024];
static uint8_t TagData[1024];
static uint8_t NewVersionData[1024];
static uint8_t VersionData[1024];
static uint8_t ValueData[KINETIC_OBJ_SIZE];
static const char* TestDataString = "lorem ipsum... blah blah blah... etc.";

//------------------------------------------------------------------------------
// Private Method Declarations
static int ParseOptions(
    const int argc,
    char** const argv,
    KineticSession * const session,
    KineticEntry* entry);
static KineticStatus ExecuteOperation(
    const char* op,
    KineticSession const * const session,
    KineticEntry* entry);
static void ConfigureEntry(
    KineticEntry* entry,
    const char* key,
    const char* tag,
    const char* version,
    KineticAlgorithm algorithm,
    const char* value);
static void ReportOperationConfiguration(
    const char* operation,
    KineticSession* session,
    KineticEntry* entry);


//------------------------------------------------------------------------------
// Main Entry Point Definition
int main(int argc, char** argv)
{
    KineticClient_Init("stdout", 2);

    // Parse command line options
    int operationsArgsIndex = ParseOptions(argc, argv, &Session, &Entry);

    // Establish a session/connection with the Kinetic Device
    KineticStatus status = KineticClient_CreateConnection(&Session);
    if (status != KINETIC_STATUS_SUCCESS) {
        printf("Failed connecting to host %s:%d (status: %s)\n\n",
               Session.config.host, Session.config.port,
               Kinetic_GetStatusDescription(status));
        return -1;
    }

    // Execute all specified operations in order
    for (int optionIndex = operationsArgsIndex; optionIndex < argc; optionIndex++) {
        const char* operation = argv[optionIndex];
        ReportOperationConfiguration(operation, &Session, &Entry);
        ExecuteOperation(operation, &Session, &Entry);
    }

    // Shutdown the Kinetic Device session
    KineticClient_DestroyConnection(&Session);
    KineticClient_Shutdown();
    printf("\nKinetic client session terminated!\n\n");

    return 0;
}


//------------------------------------------------------------------------------
// Private Method Definitions

KineticStatus ExecuteOperation(
    const char* operation,
    KineticSession const * const session,
    KineticEntry* entry)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (strcmp("instanterase", operation) == 0) {
        status = KineticClient_InstantSecureErase(session);
        if (status == 0) {
            printf("\nInstantSecureErase executed successfully."
                   " The device has been erased!\n\n");
        }
    }

    else if (strcmp("noop", operation) == 0) {
        status = KineticClient_NoOp(session);
        if (status == KINETIC_STATUS_SUCCESS) {
            printf("\nNoOp operation completed successfully."
                   " Kinetic Device is alive and well!\n\n");
        }
    }

    else if (strcmp("put", operation) == 0) {
        status = KineticClient_Put(session, entry, NULL);
        if (status == KINETIC_STATUS_SUCCESS) {
            printf("\nPut operation completed successfully."
                   " Your data has been stored!\n\n");
        }
    }

    else if (strcmp("get", operation) == 0) {
        status = KineticClient_Get(session, entry, NULL);
        if (status == 0) {
            printf("\nGet executed successfully."
                   "The entry has been retrieved!\n\n");
        }
    }

    else if (strcmp("delete", operation) == 0) {
        status = KineticClient_Delete(session, entry, NULL);
        if (status == 0) {
            printf("\nDelete executed successfully."
                   " The entry has been destroyed!\n\n");
        }
    }

    else {
        printf("\nSpecified operation '%s' is invalid!\n\n", operation);
        return -1;
    }

    // Print out status code description if operation was not successful
    if (status != KINETIC_STATUS_SUCCESS) {
        printf("\nERROR: Operation '%s' failed! with status = %s)\n\n",
               operation, Kinetic_GetStatusDescription(status));
    }

    return status;
}

void ConfigureEntry(
    KineticEntry* entry,
    const char* key,
    const char* tag,
    const char* version,
    KineticAlgorithm algorithm,
    const char* value)
{
    assert(entry != NULL);

    ByteBuffer keyBuffer = ByteBuffer_CreateAndAppendCString(KeyData, sizeof(KeyData), key);
    ByteBuffer tagBuffer = ByteBuffer_CreateAndAppendCString(TagData, sizeof(TagData), tag);
    ByteBuffer newVersionBuffer = ByteBuffer_CreateAndAppendCString(NewVersionData, sizeof(NewVersionData), version);
    ByteBuffer versionBuffer = ByteBuffer_Create(VersionData, sizeof(VersionData), 0);
    ByteBuffer valueBuffer = ByteBuffer_CreateAndAppendCString(ValueData, sizeof(ValueData), value);

    // Setup to write some test data
    *entry = (KineticEntry) {
        .key = keyBuffer,
        .tag = tagBuffer,
        .newVersion = newVersionBuffer,
        .dbVersion = versionBuffer,
        .algorithm = algorithm,
        .value = valueBuffer,
    };
}

void ReportOperationConfiguration(
    const char* operation,
    KineticSession* session,
    KineticEntry* entry)
{
    printf("\n\n"
           "================================================================================\n"
           "Executing '%s' w/sessionuration:\n"
           "================================================================================\n"
           "  host: %s\n"
           "  port: %d\n"
           "  clusterVersion: %lld\n"
           "  identity: %lld\n"
           "  key: %zd bytes\n"
           "  value: %zd bytes\n"
           "================================================================================\n",
           operation,
           session->config.host,
           session->config.port,
           (long long int)session->config.clusterVersion,
           (long long int)session->config.identity,
           entry->key.bytesUsed,
           entry->value.bytesUsed);
}

int ParseOptions(
    const int argc,
    char** const argv,
    KineticSession * const session,
    KineticEntry* entry)
{
    // Create an ArgP processor to parse arguments
    struct {
        char host[HOST_NAME_MAX];
        int port;
        int nonBlocking;
        int useTls;
        int64_t clusterVersion;
        int64_t identity;
        char hmacKey[KINETIC_MAX_KEY_LEN];
        char key[64];
        char version[64];
        char tag[64];
        KineticAlgorithm algorithm;
    } cfg = {
        .host = "localhost",
        .port = KINETIC_PORT,
        .useTls = false,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = "asdfasdf",
        .key = "SomeObjectKeyValue",
        .version = "v1.0",
        .tag = "SomeTagValue",
        .algorithm = KINETIC_ALGORITHM_SHA1,
    };

    // Create configuration for long format options
    struct option long_options[] = {
        {"non-blocking", no_argument,       &cfg.nonBlocking, true},
        {"blocking",     no_argument,       &cfg.nonBlocking, false},
        {"tls",          no_argument,       &cfg.port,        KINETIC_TLS_PORT},
        {"host",         required_argument, 0,                'h'},
        {0,              0,                 0,                0},
    };

    // Parse the options from the command line
    extern char *optarg;
    extern int optind;
    int option, optionIndex = 0;
    while ((option = getopt_long(argc, argv, "h:", long_options, &optionIndex)) != -1) {
        // Parse options until we reach the end of the argument list
        switch (option) {
        // If this option set a flag, do nothing else now
        case 0: if (long_options[optionIndex].flag != 0) {break;}
        // Configure host
        case 'h': strcpy(cfg.host, optarg); break;
        // Discard '?', since getopt_long already printed info
        case '?': break;
        // Abort upon error
        default: assert(false);
        }
    }

    // Configure session for connection
    *session = (KineticSession) {
        .config = (KineticSessionConfig) {
            .port = cfg.port,
            .clusterVersion = cfg.clusterVersion,
            .identity = cfg.identity,
            .hmacKey = ByteArray_Create(HmacData, strlen(cfg.hmacKey)),
        }
    };
    memcpy(HmacData, cfg.hmacKey, strlen(cfg.hmacKey));
    strncpy(session->config.host, cfg.host, HOST_NAME_MAX);

    // Populate and configure the entry to be used for operations
    ConfigureEntry(entry, cfg.key, cfg.tag, cfg.version, cfg.algorithm, TestDataString);

    return optind;
}
