#ifndef _KINETIC_CONNECTION_H
#define _KINETIC_CONNECTION_H

#include "KineticTypes.h"

KineticConnection KineticConnection_Create(void);
bool KineticConnection_Connect(KineticConnection* connection, const char* host, int port, bool blocking);

#endif // _KINETIC_CONNECTION_H
