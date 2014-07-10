#ifndef _KINETIC_SOCKET_H
#define _KINETIC_SOCKET_H

#include "KineticTypes.h"

int KineticSocket_Connect(const char* host, int port, bool blocking);
void KineticSocket_Close(int socket_descriptor);

#endif // _KINETIC_SOCKET_H
