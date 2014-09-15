/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "protobuf-c/protobuf-c.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#ifndef _BSD_SOURCE
    #define _BSD_SOURCE
#endif // _BSD_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

#include "socket99/socket99.h"

static void* KineticProto_Alloc(void* buf, size_t size)
{
    LOG_LOCATION;
    ByteBuffer* p = (ByteBuffer*)buf;
    LOGF(">>>> Allocating %zu bytes; used=%zu, len=%zu",
        size, p->bytesUsed, p->array.len);
    void *res = NULL;
    if ((size > 0) && (p->bytesUsed + size <= p->array.len))
    {
        // Allocate from the end of the buffer
        res = (void*)&p->array.data[p->bytesUsed];

        // Append NULL terminator to protect from access violation if abused as a string
        p->array.data[p->bytesUsed + size] = '\0';

        // Align to next long boundary after requested size + NULL terminator
        p->bytesUsed += (size + 1 + sizeof(long)) & ~(sizeof(long) - 1);
    }
    else
    {
        LOGF("Failed allocating protobuf element! used=%zu, len=%zu",
            p->bytesUsed, p->array.len);
    }
    LOGF(">>>>>>>>addr: 0x%llX", (unsigned long long)res);
    return res;
}
 
static void KineticProto_Free(void* buf, void* ignored)
{
    (void)ignored; // to eliminate unused parameter warning
    ByteBuffer* p = (ByteBuffer*)buf;
    p->bytesUsed = 0;
}


int KineticSocket_Connect(char* host, int port, bool nonBlocking)
{
    char port_str[32];
    struct addrinfo hints;
    struct addrinfo* ai_result = NULL;
    struct addrinfo* ai = NULL;
    socket99_result result;

    // Setup server address info
    socket99_config cfg = {
        .host = host,
        .port = port,
        .nonblocking = nonBlocking,
    };
    sprintf(port_str, "%d", port);

    // Open socket
    LOGF("Connecting to %s:%d", host, port);
    if (!socket99_open(&cfg, &result))
    {
        LOGF("Failed to open socket connection with host: status %d, errno %d",
            result.status, result.saved_errno);
        return -1;
    }

    // Configure the socket
    socket99_set_hints(&cfg, &hints);
    if (getaddrinfo(cfg.host, port_str, &hints, &ai_result) != 0)
    {
        LOGF("Failed to get socket address info: errno %d", errno);
        close(result.fd);
        return -1;
    }

    for (ai = ai_result; ai != NULL; ai = ai->ai_next)
    {
        int setsockopt_result;
        int enable = 1;
        int buffer_size = PDU_VALUE_MAX_LEN;

        #if defined(SO_NOSIGPIPE) && !defined(__APPLE__)
        // On BSD-like systems we can set SO_NOSIGPIPE on the socket to
        // prevent it from sending a PIPE signal and bringing down the whole
        // application if the server closes the socket forcibly
        setsockopt_result = setsockopt(result.fd,
            SOL_SOCKET, SO_NOSIGPIPE,
            &enable, sizeof(enable));
        // Allow ENOTSOCK because it allows tests to use pipes instead of
        // real sockets
        if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK)
        {
            LOG("Failed to set SO_NOSIGPIPE on socket");
            close(result.fd);
            continue;
        }
        #endif

        // Increase send buffer to PDU_VALUE_MAX_LEN
        // Note: OS allocates 2x this value for its overhead
        setsockopt_result = setsockopt(result.fd,
            SOL_SOCKET, SO_SNDBUF,
            &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1)
        {
            LOG("Error setting socket send buffer size");
            close(result.fd);
            continue;
        }

        // Increase receive buffer to PDU_VALUE_MAX_LEN
        // Note: OS allocates 2x this value for its overheadbuffer_size
        setsockopt_result = setsockopt(result.fd,
            SOL_SOCKET, SO_RCVBUF,
            &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1)
        {
            LOG("Error setting socket receive buffer size");
            close(result.fd);
            continue;
        }

        break;
    }

    freeaddrinfo(ai_result);

    if (ai == NULL)
    {
        // we went through all addresses without finding one we could bind to
        LOGF("Could not connect to %s:%d", host, port);
        return -1;
    }
    else
    {
        LOGF("Successfully connected to %s:%d (fd=%d)", host, port, result.fd);
    }

    return result.fd;
}

void KineticSocket_Close(int socket)
{
    if (socket == -1)
    {
        LOG("Not connected so no cleanup needed");
    }
    else
    {
        LOGF("Closing socket with fd=%d", socket);
        if (close(socket) == 0)
        {
            LOG("Socket closed successfully");
        }
        else
        {
            LOGF("Error closing socket file descriptor!"
                " (fd=%d, errno=%d, desc='%s')",
                socket, errno, strerror(errno));
        }
    }
}

bool KineticSocket_Read(int socket, ByteArray dest)
{
    ByteBuffer buff = BYTE_BUFFER_INIT(dest);

    LOGF("Reading %zd bytes into buffer @ 0x%zX from fd=%d",
        buff.array.len, (size_t)buff.array.data, socket);

    while(buff.bytesUsed < buff.array.len)
    {
        int status;
        fd_set readSet;
        struct timeval timeout;

        // Time out after 5 seconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);
        status = select(socket+1, &readSet, NULL, NULL, &timeout);

        if (status < 0) // Error occurred
        {
            LOGF("Failed waiting to read from socket!"
                " status=%d, errno=%d, desc='%s'",
                status, errno, strerror(errno));
            return false;
        }
        else if (status == 0) // Timeout occurred
        {
            LOG("Timed out waiting for socket data to arrive!");
            return false;
        }
        else if (status > 0) // Data available to read
        {
            // The socket is ready for reading
            status = read(socket,
                &buff.array.data[buff.bytesUsed],
                buff.array.len - buff.bytesUsed);
            // Retry if no data yet...
            if (status == -1 && 
                    (   (errno == EINTR) ||
                        (errno == EAGAIN) ||
                        (errno == EWOULDBLOCK)
                    ) )
            {
                continue;
            }
            else if (status <= 0)
            {
                LOGF("Failed to read from socket!"
                    " status=%d, errno=%d, desc='%s'",
                    status, errno, strerror(errno));
                return false;
            }
            else
            {
                buff.bytesUsed += status;
                LOGF("Received %d bytes (%zd of %zd)",
                    status, buff.bytesUsed, buff.array.len);
            }
        }
    }
    LOGF("Received %zd of %zd bytes requested", buff.bytesUsed, buff.array.len);
    return true;
}

bool KineticSocket_ReadProtobuf(int socket, KineticPDU* pdu)
{
    LOGF("Reading %zd bytes of protobuf", pdu->header.protobufLength);
    ByteArray recvArray = {
        .data = pdu->protobufRaw,
        .len = pdu->header.protobufLength
    };

    if (KineticSocket_Read(socket, recvArray))
    {
        LOG("Read completed!");

        // Protobuf-C allocator to use for received data
        ByteBuffer recvBuffer = {
            .array = (ByteArray) {
                .data = pdu->protobufRaw,
                .len = PDU_VALUE_MAX_LEN,
            },
            .bytesUsed = 0,
        };
        ProtobufCAllocator serialAllocator = {
            KineticProto_Alloc,
            KineticProto_Free,
            (void*)&recvBuffer
        };

        KineticProto* unpacked = KineticProto__unpack(&serialAllocator,
            recvBuffer.array.len, recvBuffer.array.data);
        if (unpacked == NULL)
        {
            LOG("Error unpacking incoming Kinetic protobuf message!");
        }
        else
        {
            LOG("Protobuf unpacked successfully!");
            return true;
        }
    }

    return false;
}

bool KineticSocket_Write(int socket, ByteArray src)
{
    LOGF("Writing %zu bytes to socket...", src.len);
    for (size_t count = 0; count < src.len; )
    {
        int status = write(socket,
            &src.data[count], src.len - count);
        if (status == -1 && errno == EINTR)
        {
            LOG("Write interrupted. retrying...");
            continue;
        }
        else if (status <= 0)
        {
            LOGF("Failed to write to socket! status=%d, errno=%d\n",
                status, errno);
            return false;
        }
        else
        {
            count += status;
            LOGF("Wrote %d bytes (%zu of %zu sent)",
                status, count, src.len);
        }
    }
    LOG("Write complete");

    return true;
}

bool KineticSocket_WriteProtobuf(int socket, KineticPDU* pdu)
{
    assert(pdu != NULL);
    LOGF("Writing protobuf (%zd bytes)...", pdu->header.protobufLength);
    size_t len = KineticProto__pack(&pdu->protoData.message.proto,
        pdu->protobufRaw);
    assert(len == pdu->header.protobufLength);
    ByteArray buffer = {.data = pdu->protobufRaw, .len = len};

    return KineticSocket_Write(socket, buffer);
}
