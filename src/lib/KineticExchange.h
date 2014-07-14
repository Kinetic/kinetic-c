#ifndef _KINETIC_EXCHANGE_H
#define _KINETIC_EXCHANGE_H

#include "KineticTypes.h"
#include "KineticProto.h"

typedef struct _KineticExchange
{
    // Defaults to false, since clusterVersion is optional
    // Set to true to enable clusterVersion for the exchange
    bool has_clusterVersion;

    // Optional field - default value is 0
    // The version number of this cluster definition. If this is not equal to
    // the value on the device, the request is rejected and will return a
    // `VERSION_FAILURE` `statusCode` in the `Status` message.
    int64_t clusterVersion;

    // Required field
    // The identity associated with this request. See the ACL discussion above.
    // The Kinetic Device will use this identity value to lookup the
    // HMAC key (shared secret) to verify the HMAC.
    int64_t identity;

    // Required field
    // A unique number for this connection between the source and target.
    // On the first request to the drive, this should be the time of day in
    // seconds since 1970. The drive can change this number and the client must
    // continue to use the new number and the number must remain constant
    // during the session
    int64_t connectionID;

    // Required field
    // A monotonically increasing number for each request in a TCP connection.
    int64_t sequence;
} KineticExchange;

void KineticExchange_Init(KineticExchange* const exchange, int64_t identity, int64_t connectionID);
void KineticExchange_SetClusterVersion(KineticExchange* const exchange, int64_t clusterVersion);
void KineticExchange_IncrementSequence(KineticExchange* const exchange);
void KineticExchange_ConfigureHeader(const KineticExchange* const exchange, KineticProto_Header* const header);

#endif // _KINETIC_EXCHANGE_H
