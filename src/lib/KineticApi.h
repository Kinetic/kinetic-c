#ifndef _KINETIC_API_H
#define _KINETIC_API_H

#include "KineticTypes.h"
#include "KineticConnection.h"

const KineticConnection KineticApi_Connect(void);
KineticProto_Status_StatusCode KineticApi_SendNoop(const KineticConnection* connection);

#endif // _KINETIC_API_H
