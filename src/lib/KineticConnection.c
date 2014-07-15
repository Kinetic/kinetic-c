#include "KineticConnection.h"
#include "KineticSocket.h"
#include <string.h>

KineticConnection KineticConnection_Create(void)
{
    KineticConnection connection;
    memset(&connection, 0, sizeof(connection));
    connection.Blocking = true;
    connection.FileDescriptor = -1;
    return connection;
}

bool KineticConnection_Connect(
    KineticConnection* const connection,
    const char* host, int port, bool blocking)
{
    connection->Connected = false;
    connection->Blocking = blocking;
    connection->Port = port;
    connection->FileDescriptor = -1;
    strcpy(connection->Host, host);

    connection->FileDescriptor = KineticSocket_Connect(
        connection->Host, connection->Port, blocking);
    connection->Connected = (connection->FileDescriptor >= 0);

    return connection->Connected;
}

bool KineticConnection_SendMessage(
    KineticConnection* const connection,
    KineticMessage* const message)
{
    return false;
}

bool KineticConnection_ReceiveMessage(
    KineticConnection* const connection,
    KineticMessage* const reponse)
{
    return KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;
}
