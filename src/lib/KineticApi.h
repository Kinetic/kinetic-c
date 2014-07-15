#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "KineticTypes.h"
#include "KineticProto.h"
#include "KineticConnection.h"
#include "KineticExchange.h"
#include "KineticMessage.h"

void KineticApi_Init(const char* log_file);
const KineticConnection KineticApi_Connect(const char* host, int port, bool blocking);
KineticProto_Status_StatusCode KineticApi_SendNoop(
    KineticConnection* const connection,
    KineticMessage* const request,
    KineticMessage* const response);

#endif // _KINETIC_API_H
