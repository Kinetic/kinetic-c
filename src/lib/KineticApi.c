#include "KineticApi.h"
#include "KineticLogger.h"
#include <stdio.h>

void KineticApi_Init(const char* log_file)
{
    KineticLogger_Init(log_file);
}

const KineticConnection KineticApi_Connect(const char* host, int port, bool blocking)
{
    KineticConnection connection = KineticConnection_Create();

    if (!KineticConnection_Connect(&connection, host, port, blocking))
    {
        connection.Connected = false;
        connection.FileDescriptor = -1;
        char message[64];
        sprintf(message, "Failed creating connection to %s:%d", host, port);
        LOG(message);
    }
    else
    {
        connection.Connected = true;
    }

	return connection;
}

KineticProto_Status_StatusCode KineticApi_SendNoop(
    KineticConnection* const connection,
    KineticMessage* const request,
    KineticMessage* const response)
{
    KineticProto_Status_StatusCode status = KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;

    KineticExchange_Init(request->exchange, 1234, 5678, connection);
    KineticMessage_Init(request, request->exchange);
    KineticMessage_BuildNoop(request);
    KineticConnection_SendMessage(connection, request);
    response->exchange = request->exchange;
    if (KineticConnection_ReceiveMessage(connection, response))
    {
        status = KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
    }

	return KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED;
}
