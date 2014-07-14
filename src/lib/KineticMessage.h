#ifndef _KINETIC_MESSAGE_H
#define _KINETIC_MESSAGE_H

#include "KineticTypes.h"
#include "KineticProto.h"
#include "KineticExchange.h"

void KineticMessage_Init(KineticProto* message, KineticExchange* exchange);
void KineticMessage_NOOP(KineticProto* message);

#endif // _KINETIC_MESSAGE_H
