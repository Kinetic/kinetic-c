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

#include "kinetic_client.h"
#include "kinetic_admin_client.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

typedef enum OptionID {
    OPT_NONE                       = 0,

    // Options with short IDs
    OPT_HELP                       = '?',
    OPT_LOGLEVEL                   = 'l',
    OPT_HOST                       = 'h',
    OPT_PORT                       = 'p',
    OPT_TLSPORT                    = 't',
    OPT_IDENTITY                   = 'i',
    OPT_KEY                        = 'k',
    OPT_VALUE                      = 'v',
    OPT_CLUSTERVERSION             = 'c',
    OPT_FILE                       = 'f',

    // Options with only long IDs
    OPT_HMACKEY                    = 200,
    OPT_NEWCLUSTERVERSION          = 201,
    OPT_PIN                        = 202,
    OPT_NEWPIN                     = 203,
    OPT_LOGTYPE                    = 204,
    OPT_DEVICELOGNAME              = 205,

    // Operations
    OPT_NOOP                       = 300,
    OPT_PUT                        = 301,
    OPT_GET                        = 302,
    OPT_DELETE                     = 303,
    OPT_GETNEXT                    = 304,
    OPT_GETPREVIOUS                = 305,
    OPT_GETLOG                     = 306,
    OPT_GETDEVICESPECIFICLOG       = 307,
    OPT_SETERASEPIN                = 308,
    OPT_INSTANTERASE               = 309,
    OPT_SECUREERASE                = 310,
    OPT_SETLOCKPIN                 = 311,
    OPT_LOCKDEVICE                 = 312,
    OPT_UNLOCKDEVICE               = 313,
    OPT_SETCLUSTERVERSION          = 314,
    OPT_SETACL                     = 315,
    OPT_UPDATEFIRMWARE             = 316,

    OPT_MAX
} OptionID;

struct UtilConfig {
    OptionID opID;
    int logLevel;
    KineticClient * client;
    KineticSessionConfig config;
    KineticSessionConfig adminConfig;
    KineticSession * session;
    KineticSession * adminSession;

    uint8_t hmacKeyData[1024];
    char pin[32];
    char newPin[32];
    int64_t newClusterVersion;
    char file[256];
    KineticLogInfo_Type logType;
    char deviceLogName[64];

    // KineticEntry and associated buffer data
    KineticEntry entry;
    uint8_t keyData[1024];
    uint8_t tagData[1024];
    uint8_t newVersionData[1024];
    uint8_t versionData[1024];
    uint8_t valueData[KINETIC_OBJ_SIZE];
};

#define OPT_MIN OPT_NOOP

//------------------------------------------------------------------------------
// Private Method Declarations
static int ParseOptions(
    const int argc,
    char** const argv,
    struct UtilConfig * cfg);
static KineticStatus ExecuteOperation(
    struct UtilConfig * cfg);
static void ConfigureEntry(
    struct UtilConfig * cfg,
    const char * key,
    const char * tag,
    const char * version,
    KineticAlgorithm algorithm,
    bool force,
    const char * value);
static void PrintLogInfo(KineticLogInfo_Type type, KineticLogInfo* info);
static const char* GetOptString(OptionID opt_id);


void PrintUsage(const char* exec)
{
    printf("Usage: %s --<cmd> [options...]\n", exec);
    printf("%s --help\n", exec);

    // Standard API operations
    printf("%s --noop"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --put --key <key> --value <value>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --get --key <key>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --getnext --key <key>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --getprevious --key <key>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --delete --key <key>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]\n", exec);

    // Admin API operations
    printf("%s --getlog --logtype <utilizations|temperatures|capacities|configuration|statistics|messages|limits>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --getdevicespecificlog --devicelogname <name|ID>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --setclusterversion --newclusterversion <newclusterversion>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --seterasepin --pin <oldpin> --newpin <newerasepin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --instanterase --pin <erasepin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --secureerase --pin <erasepin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --setlockpin --pin <oldpin>> <--newpin <newpin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --lockdevice --pin <lockpin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --unlockdevice --pin <lockpin>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --setacl --file <acl_json_file>"
      " [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]\n", exec);
    printf("%s --updatefirmware --file <file> --pin <pin>"
      " [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>] \n", exec);
}


//------------------------------------------------------------------------------
// Main Entry Point Definition
int main(int argc, char** argv)
{
    struct UtilConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Parse command line options
    ParseOptions(argc, argv, &cfg);
    if (cfg.opID == OPT_NONE) {
        fprintf(stderr, "No operation specified!\n");
        PrintUsage(argv[0]);
        exit(1);
    }

    // Create a client instance
    KineticClientConfig client_config = {
        .logFile = "stdout",
        .logLevel = cfg.logLevel,
    };
    cfg.client = KineticClient_Init(&client_config);
    if (cfg.client == NULL) {
        fprintf(stderr, "Failed creating a client instance!\n");
        return 1;
    }

    // Establish a session with the Kinetic Device
    KineticStatus status = KineticClient_CreateSession(&cfg.config, cfg.client, &cfg.session);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to host %s:%d (status: %s)\n",
           cfg.config.host, cfg.config.port,
           Kinetic_GetStatusDescription(status));
        return 1;
    }

    // Establish an admin session with the Kinetic Device
    cfg.adminConfig.useSsl = true;
    status = KineticClient_CreateSession(&cfg.adminConfig, cfg.client, &cfg.adminSession);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed connecting to host %s:%d (status: %s)\n",
           cfg.config.host, cfg.config.port,
           Kinetic_GetStatusDescription(status));
        return 1;
    }

    // Execute the specified operation
    // ReportConfiguration(cmd, &cfg);
    status = ExecuteOperation(&cfg);
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "Failed executing operation!\n");
        return 1;
    }

    return 0;
}

static void PrintEntry(KineticEntry * entry)
{
    assert(entry);
    entry->key.array.data[entry->key.bytesUsed] = '\0';
    entry->value.array.data[entry->value.bytesUsed] = '\0';
    printf(
        "Kinetic Entry:\n"
        "  key: %s\n"
        "  value: %s\n",
        (char*)entry->key.array.data,
        (char*)entry->value.array.data);
}

static void PrintLogInfo(KineticLogInfo_Type type, KineticLogInfo* info)
{
    size_t i;
    switch (type) {

        case KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS:
            printf("Device Utilizations:\n");
            for (i = 0; i < info->numUtilizations; i++) {
                printf("  %s: %.2f %%\n",
                    info->utilizations[i].name,
                    info->utilizations[i].value * 100.0f);
            }
            break;

        case KINETIC_DEVICE_INFO_TYPE_CAPACITIES:
            printf("Device Capacities:\n");
            printf("  nominalCapacity: %.3f MB\n", info->capacity->nominalCapacityInBytes / (1024.0f * 1024.0f));
            printf("  portionFull:     %.2f %%\n", info->capacity->portionFull * 100.0f);
            break;

        case KINETIC_DEVICE_INFO_TYPE_TEMPERATURES:
            printf("Device Temperatures:\n");
            for (i = 0; i < info->numTemperatures; i++) {
                printf("  %s: current=%.1f, min=%.1f, max=%.1f, target=%.1f\n",
                    info->temperatures[i].name,
                    info->temperatures[i].current, info->temperatures[i].minimum,
                    info->temperatures[i].maximum, info->temperatures[i].target);
            }
            break;

        case KINETIC_DEVICE_INFO_TYPE_CONFIGURATION:
            printf("Device Configuration:\n");
            printf("  vendor: %s\n", info->configuration->vendor);
            printf("  model: %s\n", info->configuration->model);
            printf("  serialNumber: %s\n", (char*)info->configuration->serialNumber.data);
            printf("  worldWideName: %s\n", (char*)info->configuration->worldWideName.data);
            printf("  version: %s\n", info->configuration->version);
            printf("  compilationDate: %s\n", info->configuration->compilationDate);
            printf("  sourceHash: %s\n", info->configuration->sourceHash);
            printf("  protocolVersion: %s\n", info->configuration->protocolVersion);
            printf("  protocolCompilationDate: %s\n", info->configuration->protocolCompilationDate);
            printf("  protocolSourceHash: %s\n", info->configuration->protocolSourceHash);
            for (i = 0; i < info->configuration->numInterfaces; i++) {
                KineticLogInfo_Interface* iface = &info->configuration->interfaces[i];
                printf("  interface: %s\n", iface->name);
                char buf[1024];
                memset(buf, 0, sizeof(buf));
                memcpy(buf, iface->MAC.data, iface->MAC.len);
                printf("    MAC: %s\n", buf);
                memset(buf, 0, sizeof(buf));
                memcpy(buf, iface->ipv4Address.data, iface->ipv4Address.len);
                printf("    ipv4Address: %s\n", buf);
                memset(buf, 0, sizeof(buf));
                memcpy(buf, iface->ipv6Address.data, iface->ipv6Address.len);
                printf("    ipv6Address: %s\n", buf);
            }
            printf("  port: %d\n", info->configuration->port);
            printf("  tlsPort: %d\n", info->configuration->tlsPort);
            break;

        case KINETIC_DEVICE_INFO_TYPE_STATISTICS:
            printf("Device Statistics:\n");
            for (i = 0; i < info->numStatistics; i++) {
                printf("  %s: count=%llu, bytes=%llu\n",
                    KineticMessageType_GetName(info->statistics[i].messageType),
                    (long long)info->statistics[i].count,
                    (long long)info->statistics[i].bytes);
            }
            break;

        case KINETIC_DEVICE_INFO_TYPE_MESSAGES:
            printf("Device Log Messages:\n"); {
                char* msgs = calloc(1, info->messages.len + 1);
                memcpy(msgs, info->messages.data, info->messages.len);
                printf("  %s\n", msgs);
                free(msgs);
            }
            break;

        case KINETIC_DEVICE_INFO_TYPE_LIMITS:
            printf("Device Limits:\n");
            printf("  maxKeySize: %u\n",                  info->limits->maxKeySize);
            printf("  maxValueSize: %u\n",                info->limits->maxValueSize);
            printf("  maxVersionSize: %u\n",              info->limits->maxVersionSize);
            printf("  maxTagSize: %u\n",                  info->limits->maxTagSize);
            printf("  maxConnections: %u\n",              info->limits->maxConnections);
            printf("  maxOutstandingReadRequests: %u\n",  info->limits->maxOutstandingReadRequests);
            printf("  maxOutstandingWriteRequests: %u\n", info->limits->maxOutstandingWriteRequests);
            printf("  maxMessageSize: %u\n",              info->limits->maxMessageSize);
            printf("  maxKeyRangeCount: %u\n",            info->limits->maxKeyRangeCount);
            printf("  maxIdentityCount: %u\n",            info->limits->maxIdentityCount);
            printf("  maxPinSize: %u\n",                  info->limits->maxPinSize);
            break;

        default:
            fprintf(stderr, "Unknown log type! (%d)\n", type);
            break;
    }
}

static const char* GetOptString(OptionID opt_id)
{
    char* str;
    switch (opt_id) {
        case OPT_HELP:
            str = "help"; break;
        case OPT_LOGLEVEL:
            str = "loglevel"; break;
        case OPT_HOST:
            str = "host"; break;
        case OPT_PORT:
            str = "port"; break;
        case OPT_TLSPORT:
            str = "tlsport"; break;
        case OPT_IDENTITY:
            str = "identity"; break;
        case OPT_KEY:
            str = "key"; break;
        case OPT_VALUE:
            str = "value"; break;
        case OPT_CLUSTERVERSION:
            str = "clusterversion"; break;
        case OPT_FILE:
            str = "file"; break;
        case OPT_HMACKEY:
            str = "hmackey"; break;
        case OPT_NEWCLUSTERVERSION:
            str = "newclusterversion"; break;
        case OPT_PIN:
            str = "pin"; break;
        case OPT_NEWPIN:
            str = "newpin"; break;
        case OPT_LOGTYPE:
            str = "logtype"; break;
        case OPT_NOOP:
            str = "noop"; break;
        case OPT_PUT:
            str = "put"; break;
        case OPT_GET:
            str = "get"; break;
        case OPT_DELETE:
            str = "delete"; break;
        case OPT_GETNEXT:
            str = "getnext"; break;
        case OPT_GETPREVIOUS:
            str = "getprevious"; break;
        case OPT_GETLOG:
            str = "getlog"; break;
        case OPT_GETDEVICESPECIFICLOG:
            str = "getdevicespecificlog"; break;
        case OPT_SETERASEPIN:
            str = "seterasepin"; break;
        case OPT_INSTANTERASE:
            str = "instanterase"; break;
        case OPT_SECUREERASE:
            str = "secureerase"; break;
        case OPT_SETLOCKPIN:
            str = "setlockpin"; break;
        case OPT_LOCKDEVICE:
            str = "lockdevice"; break;
        case OPT_UNLOCKDEVICE:
            str = "unlockdevice"; break;
        case OPT_SETCLUSTERVERSION:
            str = "setclusterversion"; break;
        case OPT_SETACL:
            str = "setacl"; break;
        case OPT_UPDATEFIRMWARE:
            str = "updatefirmware"; break;

        default:
            str = "<UNKNOWN OPTION>"; break;
    };
    return str;
}


//------------------------------------------------------------------------------
// Private Method Definitions

KineticStatus ExecuteOperation(
    struct UtilConfig * cfg)
{
    KineticStatus status = KINETIC_STATUS_INVALID;
    KineticLogInfo * logInfo;
    ByteArray tmpArray;

    switch (cfg->opID) {

        case OPT_NOOP:
            status = KineticClient_NoOp(cfg->session);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("NoOp operation completed successfully."
                    " Kinetic Device is alive and well!\n"); }
            break;

        case OPT_PUT:
            status = KineticClient_Put(cfg->session, &cfg->entry, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("Put operation completed successfully."
                       " Your data has been stored!\n");
                PrintEntry(&cfg->entry);
            }
            break;

        case OPT_GET:
            status = KineticClient_Get(cfg->session, &cfg->entry, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("Get executed successfully.\n");
                PrintEntry(&cfg->entry);
            }
            break;

        case OPT_GETNEXT:
            status = KineticClient_GetNext(cfg->session, &cfg->entry, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("GetNext executed successfully.\n");
                PrintEntry(&cfg->entry);
            }
            break;

        case OPT_GETPREVIOUS:
            status = KineticClient_GetPrevious(cfg->session, &cfg->entry, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("GetPrevious executed successfully.\n");
                PrintEntry(&cfg->entry);
            }
            break;

        case OPT_DELETE:
            status = KineticClient_Delete(cfg->session, &cfg->entry, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("Delete executed successfully. The entry has been destroyed!\n");
                PrintEntry(&cfg->entry);
            }
            break;

        case OPT_GETLOG:
            status = KineticAdminClient_GetLog(cfg->adminSession, cfg->logType, &logInfo, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("GetLog executed successfully.\n"
                       "The device log info has been retrieved!\n");
                PrintLogInfo(cfg->logType, logInfo);
            }
            break;

        case OPT_GETDEVICESPECIFICLOG:
            if (strlen(cfg->deviceLogName) == 0) {
                fprintf(stderr, "Device-specific log type requires '--devicelogname' to be set!\n");
                exit(1);
            }
            tmpArray.data = (uint8_t*)cfg->deviceLogName;
            tmpArray.len = strlen(cfg->deviceLogName);
            status = KineticAdminClient_GetDeviceSpecificLog(cfg->adminSession, tmpArray, &logInfo, NULL);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("GetDeviceSpecificLog executed successfully."
                       "The device-specific device log info has been retrieved!\n");
                printf("Device-Specific Log Info:\n");
                char* dev = calloc(1, logInfo->device->name.len + 1);
                memcpy(dev, logInfo->device->name.data, logInfo->device->name.len);
                printf("  %s\n", dev);
                free(dev);
            }
            else if (status == KINETIC_STATUS_NOT_FOUND) {
                fprintf(stderr, "The specified device-specific log '%s' was not found on the device!\n", cfg->deviceLogName);
                status = KINETIC_STATUS_SUCCESS;
            }
            break;

        case OPT_SETERASEPIN:
            status = KineticAdminClient_SetErasePin(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)),
                ByteArray_Create(cfg->newPin, strlen(cfg->newPin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SetErasePin executed successfully.\n"
                       "The kinetic device erase pin has been changed!\n"); }
            break;

        case OPT_INSTANTERASE:
            status = KineticAdminClient_InstantErase(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("InstantErase executed successfully.\n"
                       "The kinetic device has been erased!\n"); }
            break;

        case OPT_SECUREERASE:
            status = KineticAdminClient_SecureErase(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SecureErase executed successfully.\n"
                       "The kinetic device has been erased!\n"); }
            break;

        case OPT_SETLOCKPIN:
            status = KineticAdminClient_SetLockPin(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)),
                ByteArray_Create(cfg->newPin, strlen(cfg->newPin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SetLockPin executed successfully.\n"
                       "The kinetic device lock/unlock pin has been changed!\n"); }
            break;

        case OPT_LOCKDEVICE:
            status = KineticAdminClient_LockDevice(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("LockDevice executed successfully.\n"
                       "The kinetic device is now locked!\n"); }
            break;

        case OPT_UNLOCKDEVICE:
            status = KineticAdminClient_UnlockDevice(cfg->adminSession,
                ByteArray_Create(cfg->pin, strlen(cfg->pin)));
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("UnlockDevice executed successfully.\n"
                       "The kinetic device is now unlocked!\n"); }
            break;

        case OPT_SETCLUSTERVERSION:
            status = KineticAdminClient_SetClusterVersion(cfg->adminSession, cfg->newClusterVersion);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SetClusterVersion executed successfully.\n"
                       "The kinetic device's cluster version has been updated!\n"); }
            break;

        case OPT_SETACL:
            status = KineticAdminClient_SetACL(cfg->adminSession, cfg->file);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SetACL executed successfully.\n"
                       "The kinetic device ACLs have been replaced!\n"); }
            break;

        case OPT_UPDATEFIRMWARE:
            status = KineticAdminClient_UpdateFirmware(cfg->session, cfg->file);
            if (status == KINETIC_STATUS_SUCCESS) {
                printf("SecureErase executed successfully.\n"
                       "The kinetic device has been restored to empty status!\n"); }
            break;

        default:
            fprintf(stderr, "Specified operation '%d' is invalid!\n",
                (int)cfg->opID);
            exit(-1);
    };

    // Print out status code description if operation was not successful
    if (status != KINETIC_STATUS_SUCCESS) {
        fprintf(stderr, "\nERROR: Operation '%s' failed with status '%s'\n",
            GetOptString(cfg->opID), Kinetic_GetStatusDescription(status));
    }

    return status;
}

void ConfigureEntry(struct UtilConfig * cfg, const char* key, const char* tag,
    const char* version, KineticAlgorithm algorithm, bool force, const char* value)
{
    assert(cfg != NULL);

    // Setup to write some test data
    cfg->entry = (KineticEntry) {
        .key = ByteBuffer_CreateAndAppendCString(cfg->keyData, sizeof(cfg->keyData), key),
        .tag = ByteBuffer_CreateAndAppendCString(cfg->tagData, sizeof(cfg->tagData), tag),
        .newVersion = ByteBuffer_CreateAndAppendCString(cfg->newVersionData, sizeof(cfg->newVersionData), version),
        .dbVersion = ByteBuffer_Create(cfg->versionData, sizeof(cfg->versionData), 0),
        .algorithm = algorithm,
        .value = ByteBuffer_CreateAndAppendCString(cfg->valueData, sizeof(cfg->valueData), value),
        .force = force,
    };
}

int ParseOptions(
    const int argc,
    char** const argv,
    struct UtilConfig * cfg)
{
    // Create an ArgP processor to parse arguments
    struct {
        OptionID opID;
        int logLevel;
        char host[HOST_NAME_MAX];
        int port;
        int tlsPort;
        int64_t clusterVersion;
        int64_t identity;
        char hmacKey[KINETIC_MAX_KEY_LEN];
        char logType[64];
        char deviceLogName[64];
        char key[64];
        char version[64];
        char tag[64];
        KineticAlgorithm algorithm;
        bool force;
        char value[1024];
    } opts = {
        .logLevel = 0,
        .host = "localhost",
        .port = KINETIC_PORT,
        .tlsPort = KINETIC_TLS_PORT,
        .clusterVersion = 0,
        .identity = 1,
        .hmacKey = "asdfasdf",
        .logType = "utilizations",
        .key = "foo",
        .tag = "SomeTagValue",
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .force = true,
        .value = "Hello!",
    };

    // Create configuration for long format options
    struct option long_options[] = {

        // Help
        {"help",                        no_argument,       0, OPT_HELP},

        // Commands
        {"noop",                        no_argument,       0, OPT_NOOP},
        {"put",                         no_argument,       0, OPT_PUT},
        {"get",                         no_argument,       0, OPT_GET},
        {"delete",                      no_argument,       0, OPT_DELETE},
        {"getnext",                     no_argument,       0, OPT_GETNEXT},
        {"getprevious",                 no_argument,       0, OPT_GETPREVIOUS},
        {"getlog",                      no_argument,       0, OPT_GETLOG},
        {"getdevicespecificlog",        no_argument,       0, OPT_GETDEVICESPECIFICLOG},
        {"seterasepin",                 no_argument,       0, OPT_SETERASEPIN},
        {"instanterase",                no_argument,       0, OPT_INSTANTERASE},
        {"secureerase",                 no_argument,       0, OPT_SECUREERASE},
        {"setlockpin",                  no_argument,       0, OPT_SETLOCKPIN},
        {"lockdevice",                  no_argument,       0, OPT_LOCKDEVICE},
        {"unlockdevice",                no_argument,       0, OPT_UNLOCKDEVICE},
        {"setclusterversion",           no_argument,       0, OPT_SETCLUSTERVERSION},
        {"setacl",                      no_argument,       0, OPT_SETACL},
        {"updatefirmware",              no_argument,       0, OPT_UPDATEFIRMWARE},

        // Options
        {"loglevel",                    required_argument, 0, OPT_LOGLEVEL},
        {"host",                        required_argument, 0, OPT_HOST},
        {"port",                        required_argument, 0, OPT_PORT},
        {"tlsport",                     required_argument, 0, OPT_TLSPORT},
        {"identity",                    required_argument, 0, OPT_IDENTITY},
        {"hmackey",                     required_argument, 0, OPT_HMACKEY},
        {"clusterversion",              required_argument, 0, OPT_CLUSTERVERSION},
        {"file",                        required_argument, 0, OPT_FILE},
        {"newclusterversion",           required_argument, 0, OPT_NEWCLUSTERVERSION},
        {"pin",                         required_argument, 0, OPT_PIN},
        {"newpin",                      required_argument, 0, OPT_NEWPIN},
        {"logtype",                     required_argument, 0, OPT_LOGTYPE},
        {"devicelogname",               required_argument, 0, OPT_DEVICELOGNAME},
        {"key",                         required_argument, 0, OPT_KEY},
        {"value",                       required_argument, 0, OPT_VALUE},
        {0,                             0,                 0, 0},
    };

    // Parse the options from the command line
    extern char *optarg;
    extern int optind;
    int option, optionIndex = 0;
    while ((option = getopt_long(argc, argv, "?lhptics:", long_options, &optionIndex)) != -1) {
        switch (option) {
            case 0:
                // If this option, just set the flag
                if (long_options[optionIndex].flag != 0) {break;}
                // assert(false);
                // break;
            case OPT_LOGLEVEL:
                opts.logLevel = atoi(optarg);
                break;
            case OPT_HOST:
                strncpy(opts.host, optarg, sizeof(opts.host));
                break;
            case OPT_PORT:
                opts.port = atoi(optarg);
                break;
            case OPT_TLSPORT:
                opts.tlsPort = atoi(optarg);
                break;
            case OPT_IDENTITY:
                opts.identity = atoi(optarg);
                break;
            case OPT_HMACKEY:
                strncpy(opts.hmacKey, optarg, sizeof(opts.hmacKey)-1);
                break;
            case OPT_KEY:
                strncpy(opts.key, optarg, sizeof(opts.key)-1);
                break;
            case OPT_VALUE:
                strncpy(opts.value, optarg, sizeof(opts.value)-1);
                break;
            case OPT_CLUSTERVERSION:
                opts.clusterVersion = (int64_t)atol(optarg);
                break;
            case OPT_NEWCLUSTERVERSION:
                cfg->newClusterVersion = (int64_t)atol(optarg);
                break;
            case OPT_FILE:
                strncpy(cfg->file, optarg, sizeof(cfg->file)-1);
                break;
            case OPT_PIN:
                strncpy(cfg->pin, optarg, sizeof(cfg->pin)-1);
                break;
            case OPT_NEWPIN:
                strncpy(cfg->newPin, optarg, sizeof(cfg->newPin)-1);
                break;
            case OPT_LOGTYPE:
                strncpy(opts.logType, optarg, sizeof(opts.logType)-1);
                break;
            case OPT_DEVICELOGNAME:
                strncpy(cfg->deviceLogName, optarg, sizeof(cfg->deviceLogName)-1);
                break;

            case OPT_NOOP:
            case OPT_PUT:
            case OPT_GET:
            case OPT_DELETE:
            case OPT_GETNEXT:
            case OPT_GETPREVIOUS:
            case OPT_GETLOG:
            case OPT_GETDEVICESPECIFICLOG:
            case OPT_SETERASEPIN:
            case OPT_INSTANTERASE:
            case OPT_SECUREERASE:
            case OPT_SETLOCKPIN:
            case OPT_LOCKDEVICE:
            case OPT_UNLOCKDEVICE:
            case OPT_SETCLUSTERVERSION:
            case OPT_SETACL:
            case OPT_UPDATEFIRMWARE:
                if ((int)opts.opID == 0) {
                    opts.opID = option;
                    break;
                }
                fprintf(stderr, "Multiple operations specified!\n");
                PrintUsage(argv[0]);
                exit(-1);
            case OPT_HELP:
                PrintUsage(argv[0]);
                exit(0);
            default:
                PrintUsage(argv[0]);
                exit(-1);
        }
    }

    // Configure client
    cfg->logLevel = opts.logLevel;

    // Configure session
    cfg->config = (KineticSessionConfig) {
        .port = opts.port,
        .clusterVersion = opts.clusterVersion,
        .identity = opts.identity,
        .hmacKey = ByteArray_Create(cfg->hmacKeyData, strlen(opts.hmacKey)),
    };
    memcpy(cfg->hmacKeyData, opts.hmacKey, strlen(opts.hmacKey));
    strncpy(cfg->config.host, opts.host, sizeof(cfg->config.host)-1);

    // Configure admin session
    cfg->adminConfig = cfg->config;
    cfg->adminConfig.port = opts.tlsPort;
    cfg->adminConfig.useSsl = true;

    // Populate and configure the entry to be used for operations
    ConfigureEntry(cfg,
        opts.key, opts.tag, opts.version, opts.algorithm, opts.force, opts.value);

    cfg->opID = opts.opID;

    // Parse log type from string
    if (strcmp("utilizations", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_UTILIZATIONS;
    }
    else if (strcmp("temperatures", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_TEMPERATURES;
    }
    else if (strcmp("capacities", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_CAPACITIES;
    }
    else if (strcmp("configuration", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_CONFIGURATION;
    }
    else if (strcmp("statistics", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_STATISTICS;
    }
    else if (strcmp("messages", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_MESSAGES;
    }
    else if (strcmp("limits", opts.logType) == 0) {
        cfg->logType = KINETIC_DEVICE_INFO_TYPE_LIMITS;
    }
    else {
        fprintf(stderr, "Invalid log info type: %s\n", opts.logType);
        exit(1);
    }

    return optind;
}
