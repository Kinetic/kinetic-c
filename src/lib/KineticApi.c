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
    KineticConnection* connection,
    KineticExchange* exchange,
    KineticMessage* message)
{
    KineticExchange_Init(exchange, 1234, 5678);
    KineticMessage_Init(message, exchange);
    KineticConnection_SendMessage(connection, message);

	return KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED;
}
