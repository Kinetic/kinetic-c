#include "KineticApi.h"
#include "KineticConnection.h"

const KineticConnection KineticApi_Connect(void)
{
	return KineticConnection_Create();
}

KineticProto_Status_StatusCode KineticApi_SendNoop(const KineticConnection* connection)
{
	return KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
}
