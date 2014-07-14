#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "KineticTypes.h"
#include "KineticConnection.h"
#include "KineticExchange.h"
#include "KineticMessage.h"

void KineticApi_Init(const char* log_file);
const KineticConnection KineticApi_Connect(const char* host, int port, bool blocking);
KineticProto_Status_StatusCode KineticApi_SendNoop(KineticConnection* connection, KineticExchange* exchange, KineticMessage* message);

#endif // _KINETIC_API_H
