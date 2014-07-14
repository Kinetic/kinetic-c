#ifndef _KINETIC_CONNECTION_H
#define _KINETIC_CONNECTION_H

#include "KineticTypes.h"
#include "KineticMessage.h"

typedef struct _KineticConnection
{
    bool    Connected;
    bool    Blocking;
    int     Port;
    int     FileDescriptor;
    char    Host[HOST_NAME_MAX];
} KineticConnection;

KineticConnection KineticConnection_Create(void);
bool KineticConnection_Connect(KineticConnection* connection, const char* host, int port, bool blocking);
bool KineticConnection_SendMessage(KineticConnection* connection, KineticMessage* message);

#endif // _KINETIC_CONNECTION_H
