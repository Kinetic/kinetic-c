#ifndef _KINETIC_PDU_H
#define _KINETIC_PDU_H

#include "KineticProto.h"

#define PDU_HEADER_LEN      (1 + (2 * sizeof(int64_t)))
#define PDU_PROTO_MAX_LEN   (1024 * 1024)
#define PDU_VALUE_MAX_LEN   (1024 * 1024)
#define PDU_MAX_LEN         (PDU_HEADER_LEN + PDU_PROTO_MAX_LEN + PDU_VALUE_MAX_LEN)

typedef struct _KineticPDU
{
    uint8_t* prefix;
    uint8_t* protoLength;
    uint8_t* valueLength;
    uint8_t* proto;
    uint8_t* value;
    uint8_t* data;
} KineticPDU;

void KineticPDU_Create(
    KineticPDU* const pdu,
    uint8_t* const buffer,
    const KineticProto* const proto,
    const uint8_t* const value,
    const int64_t valueLength);

#endif // _KINETIC_PDU_H
