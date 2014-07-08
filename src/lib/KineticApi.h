#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "KineticTypes.h"

void KineticApi_Init(const char* log_file);
const KineticConnection KineticApi_Connect(const char* host, int port, bool blocking);
KineticProto_Status_StatusCode KineticApi_SendNoop(const KineticConnection* connection);

#endif // _KINETIC_API_H
