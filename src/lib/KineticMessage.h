#ifndef _KINETIC_MESSAGE_H
#define _KINETIC_MESSAGE_H

#include "KineticTypes.h"
#include "KineticProto.h"
#include "KineticExchange.h"

typedef struct _KineticMessage
{
    // Kinetic Protocol Buffer Elements
    KineticProto            proto;
    KineticProto_Command    command;
    KineticProto_Header     header;
    KineticProto_Body       body;
    KineticProto_Status     status;

    // Other pertinent data members
    KineticExchange*        exchange;
} KineticMessage;

void KineticMessage_Init(KineticMessage* message, KineticExchange* exchange);

#endif // _KINETIC_MESSAGE_H
