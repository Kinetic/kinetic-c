#include "KineticExchange.h"
#include <string.h>

void KineticExchange_Init(
    KineticExchange* const exchange,
    int64_t identity,
    int64_t connectionID,
    KineticConnection* const connection)
{
    memset(exchange, 0, sizeof(KineticExchange));
    exchange->identity = identity;
    exchange->connectionID = connectionID;
    exchange->connection = connection;
}

void KineticExchange_SetClusterVersion(
    KineticExchange* const exchange,
    int64_t clusterVersion)
{
    exchange->clusterVersion = clusterVersion;
    exchange->has_clusterVersion = true;
}

void KineticExchange_IncrementSequence(
    KineticExchange* const exchange)
{
    exchange->sequence++;
}

void KineticExchange_ConfigureHeader(
    const KineticExchange* const exchange,
    KineticProto_Header* const header)
{
    header->has_clusterversion = exchange->has_clusterVersion;
    if (exchange->has_clusterVersion)
    {
        header->clusterversion = exchange->clusterVersion;
    }
    header->has_identity = true;
    header->identity = exchange->identity;
    header->has_connectionid = true;
    header->connectionid = exchange->connectionID;
    header->has_sequence = true;
    header->sequence = exchange->sequence;
}
