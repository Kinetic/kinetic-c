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

bool KineticConnection_Connect(KineticConnection* connection, const char* host, int port, bool blocking)
{
    connection->Connected = false;
    connection->Blocking = blocking;
    connection->Port = port;
    connection->FileDescriptor = -1;
    strcpy(connection->Host, host);

    connection->FileDescriptor = KineticSocket_Connect(connection->Host, connection->Port, blocking);
    connection->Connected = (connection->FileDescriptor >= 0);

    return connection->Connected;
}

bool KineticConnection_SendMessage(KineticConnection* connection, KineticMessage* message)
{
    connection = connection;
    message = message;
    return false;
}
