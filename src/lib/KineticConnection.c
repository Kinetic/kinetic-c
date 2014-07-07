#include "KineticConnection.h"
#include "KineticNetwork.h"
#include <netinet/in.h>
#include <string.h>

KineticConnection KineticConnection_Create(void)
{
	KineticConnection connection;
	memset(&connection, 0, sizeof(connection));
	return connection;
}

KineticProto_Status_StatusCode KineticConnection_Connect(KineticConnection* connection)
{
	struct ifaddrs addr = KineticNetwork_GetDestinationIP();
	connection->Connected = true;
	strcpy(connection->Name, addr.ifa_name);
	connection->Flags = addr.ifa_flags;
	memcpy(&connection->Addr, addr.ifa_addr, sizeof(struct sockaddr));
	memcpy(&connection->NetMask, addr.ifa_netmask, sizeof(struct sockaddr));
	memcpy(&connection->DestAddr, addr.ifa_dstaddr, sizeof(struct sockaddr));
	return KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS;
}
