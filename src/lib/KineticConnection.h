#ifndef _KINETIC_CONNECTION_H
#define _KINETIC_CONNECTION_H

#include "KineticTypes.h"
#include "KineticMessage.h"

KineticConnection KineticConnection_Create(void);
bool KineticConnection_Connect(KineticConnection* const connection, const char* host, int port, bool blocking);
bool KineticConnection_SendMessage(KineticConnection* const connection, KineticMessage* const message);
bool KineticConnection_ReceiveMessage(KineticConnection* const connection, KineticMessage* const reponse);

#endif // _KINETIC_CONNECTION_H
