#ifndef _KINETIC_CONNECTION_H
#define _KINETIC_CONNECTION_H

#include "KineticTypes.h"

#define MAX_NET_NAME 128

typedef struct _KineticConnection
{
	bool Connected;
	char Name[MAX_NET_NAME];
	unsigned int Flags;
	struct sockaddr Addr;
	struct sockaddr NetMask;
	struct sockaddr DestAddr;
} KineticConnection;

KineticConnection KineticConnection_Create(void);
KineticProto_Status_StatusCode KineticConnection_Connect(KineticConnection* connection);

#endif // _KINETIC_CONNECTION_H
