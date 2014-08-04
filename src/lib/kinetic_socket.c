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
#include <protobuf-c/protobuf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int KineticSocket_GetSocketError(int fd)
{
    int err = 1;
    socklen_t len = sizeof err;
    
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len) == -1)
    {
        LOG("Failed to get socket options!");
    }

    if (err)
    {
        errno = err; // set errno to the socket SO_ERROR
    }

    return err;
}

bool KineticSocket_HaveInput(int fd, double timeout)
{
    int status;
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec  = (long)timeout; // cast needed for C++
    tv.tv_usec = (long)((timeout - tv.tv_sec) * 1000000); // 'suseconds_t'

    while (1)
    {
        if (!(status = select(fd + 1, &fds, 0, 0, &tv)))
        {
            return false;
        }
        else if (status > 0 && FD_ISSET(fd, &fds))
        {
            return true;
        }
        else if (status > 0)
        {
            LOG("I am confused!!!");
        }
        else if (errno != EINTR)
        {
            LOG("Failed select upon flush!"); // tbd EBADF: man page "an error has occurred"
        }
    }
}

bool KineticSocket_FlushSocketBeforeClose(int fd/*, double timeout*/)
{
    // const double start = getWallTimeEpoch();
    char discard[99];
    int i, iterations = 5;

    assert(SHUT_WR == 1);

    if (shutdown(fd, 1) != -1)
    {
        for (i = 0; i < iterations; i++)
        {
            while (KineticSocket_HaveInput(fd, 0.01)) // can block for 0.01 secs
            {
                if (!read(fd, discard, sizeof discard))
                {
                    return true; // success!
                }
            }
        }
    }

   return false;
}

bool KineticSocket_CloseSocket(int fd)
{
    if (fd >= 0)
    {
        char msg[64];
        int err;

        LOG("KineticSocket_CloseSocket");

        if (!KineticSocket_FlushSocketBeforeClose(fd))
        {
            LOG("Failed to gracefully close socket!");
        }
        else
        {
            LOG("Socket flushed successfully");
        }
        
        err = KineticSocket_GetSocketError(fd); // first clear any errors, which can cause close to fail
        if (err)
        {
            sprintf(msg, "Socket error had occurred: err=%d", err);
            LOG(msg);
        }
        else
        {
            LOG("Socket errors cleared successfully!");
        }
        
        if (shutdown(fd, SHUT_RDWR) < 0) // secondly, terminate the 'reliable' delivery
        {
            if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
            {
                sprintf(msg, "Socket error occurred! errno=%d", errno);
                return false;
            }

            if (close(fd) < 0) // finally call close()
            {
                LOG("Failed closing socket!");
                return false;
            }
            else
            {
                LOG("Succeeded closing socket");
            }
        }
        else
        {
            LOG("Call to shutdown socket failed!");
            return false;
        }
    }

    return true;
}

int KineticSocket_Connect(const char* host, int port, bool blocking)
{
    char message[256];
    char port_str[32];
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    struct addrinfo* ai = NULL;
    int socket_fd;
    int res;

    sprintf(message, "\nConnecting to %s:%d", host, port);
    LOG(message);

    memset(&hints, 0, sizeof(struct addrinfo));

    // could be inet or inet6
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;

    sprintf(port_str, "%d", port);

    res = getaddrinfo(host, port_str, &hints, &result);
    if (res != 0)
    {
        LOG("Could not resolve host");
        return -1;
    }

    for (ai = result; ai != NULL; ai = ai->ai_next)
    {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        int res;

        res = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, strlen(host), service,
                sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
        if (res != 0)
        {
            LOG("Could not get name info");
            continue;
        }
        else
        {
            LOG("Trying to connect to host");
        }

        socket_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (socket_fd == -1)
        {
            LOG("Could not create socket");
            continue;
        }

        // os x won't let us set close-on-exec when creating the socket, so set it separately
        int current_fd_flags = fcntl(socket_fd, F_GETFD);
        if (current_fd_flags == -1)
        {
            LOG("Failed to get socket fd flags");
            close(socket_fd);
            continue;
        }
        if (fcntl(socket_fd, F_SETFD, current_fd_flags | FD_CLOEXEC) == -1)
        {
            LOG("Failed to set socket close-on-exit");
            close(socket_fd);
            continue;
        }

        // On BSD-like systems we can set SO_NOSIGPIPE on the socket to prevent it from sending a
        // PIPE signal and bringing down the whole application if the server closes the socket
        // forcibly
// #ifdef SO_NOSIGPIPE
//         int set = 1;
//         int setsockopt_result = setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
//         // Allow ENOTSOCK because it allows tests to use pipes instead of real sockets
//         if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK)
//         {
//             LOG("Failed to set SO_NOSIGPIPE on socket");
//             close(socket_fd);
//             continue;
//         }
// #endif

        if (connect(socket_fd, ai->ai_addr, ai->ai_addrlen) == -1)
        {
            LOG("Unable to connect");
            close(socket_fd);
            continue;
        }
        else
        {
            LOG("Connected to host!");
        }

        if (!blocking && fcntl(socket_fd, F_SETFL, O_NONBLOCK) != 0)
        {
            LOG("Failed to set socket nonblocking");
            close(socket_fd);
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (ai == NULL)
    {
        // we went through all addresses without finding one we could bind to
        sprintf(message, "Could not connect to %s:%d", host, port);
        LOG(message);
        return -1;
    }

    return socket_fd;
}

void KineticSocket_Close(int socketDescriptor)
{
    char message[64];
    if (socketDescriptor == -1)
    {
        LOG("Not connected so no cleanup needed");
    }
    else
    {
        sprintf(message, "Closing socket with fd=%d", socketDescriptor);
        LOG(message);
        if (KineticSocket_CloseSocket(socketDescriptor))
        {
            LOG("Socket closed successfully");
        }
        else
        {
            LOG("Error closing socket file descriptor");
        }
    }
}

bool KineticSocket_Read(int socketDescriptor, void* buffer, size_t length)
{
    size_t count;
    uint8_t* data = (uint8_t*)buffer;

    for (count = 0; count < length; )
    {
        int status;
        fd_set readSet;
        struct timeval timeout;

        // Time out after 5 seconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&readSet);
        FD_SET(socketDescriptor, &readSet);
        status = select(socketDescriptor+1, &readSet, NULL, NULL, &timeout);

        if (status < 0) // Error occurred
        {
            char msg[128];
            sprintf(msg, "Failed waiting to read from socket! status=%d, errno=%d\n", status, errno);
            LOG(msg);
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
            status = read(socketDescriptor, &buffer[count], length - count);
            if (status == -1 && errno == EINTR)
            {
                continue;
            }
            else if (status <= 0)
            {
                char msg[128];
                sprintf(msg, "Failed to read from socket! status=%d, errno=%d\n", status, errno);
                LOG(msg);
                return false;
            }
            else
            {
                count += status;
            }
        }
    }

    return true;
}

bool KineticSocket_ReadProtobuf(int socketDescriptor, KineticProto** message, void* buffer, size_t length)
{
    bool success = false;
    char msg[128];
    KineticProto* unpacked = NULL;

    LOG("Attempting to read protobuf...");
    sprintf(msg, "Reading %zd bytes into buffer @ 0x%X from fd=%d", length, (unsigned int)buffer, socketDescriptor);
    LOG(msg);

    if (KineticSocket_Read(socketDescriptor, buffer, length))
    {
        LOG("Read completed!");
        unpacked = KineticProto_unpack(NULL, length, buffer);
        if (unpacked == NULL)
        {
            LOG("Error unpacking incoming Kinetic protobuf message!");
            *message = NULL;
            success = false;
        }
        else
        {
            LOG("Protobuf unpacked successfully!");
            *message = unpacked;
            success = true;
        }
    }
    else
    {
        *message = NULL;
        success = false;
    }

    return success;
}

bool KineticSocket_Write(int socketDescriptor, const void* buffer, size_t length)
{
    int status;
    size_t count;
    uint8_t* data = (uint8_t*)buffer;
    char msg[128];

    for (count = 0; count < length; )
    {
        LOG("Sending data to client...");
        sprintf(msg, "Attempting to write %zd bytes to socket", (length - count));
        LOG(msg);
        status = write(socketDescriptor, &buffer[count], length - count);
        if (status == -1 && errno == EINTR)
        {
            LOG("Write interrupted... retrying...");
            continue;
        }
        else if (status <= 0)
        {
            LOG("Failed writing data to socket!");
            sprintf(msg, "Failed to write to socket! status=%d, errno=%d\n", status, errno);
            LOG(msg);
            return false;
        }
        else
        {
            count += status;
            sprintf(msg, "Wrote %zd bytes to socket! %zd more to send...", status, (length - count));
            LOG(msg);
        }
    }

    LOG("Write completed successfully!");
    return true;
}


// protobuf-c ProtobufCBuffer for serializing direct to a file
typedef struct _ProtobufAppendToFile
{
  ProtobufCBuffer base;     /* 'base' must be first member of Buffer subclass */
  int fileHandle;
  protobuf_c_boolean error;
} ProtobufAppendToFile;

static void KineticSocket_AppendProtobufToFile(ProtobufCBuffer* buffer, size_t length, const uint8_t* data)
{
    ProtobufAppendToFile *fileBuffer = (ProtobufAppendToFile*)buffer;
    if (!fileBuffer->error)
    {
        fileBuffer->error = !KineticSocket_Write(fileBuffer->fileHandle, data, length);
    }
}

bool KineticSocket_WriteProtobuf(int socketDescriptor, const KineticMessage* message)
{
    bool success = false;
    ProtobufAppendToFile appender;
    size_t len = KineticProto_get_packed_size(&message->proto);
    char msg[64];

    sprintf(msg, "Writing protobuf (%zd bytes)", len);
    LOG(msg);
    appender.base.append = KineticSocket_AppendProtobufToFile;
    appender.fileHandle = socketDescriptor;
    appender.error = false;

    // FIXME!!! protobuf-c appears to be prepending an extra 2 bytes to the
    // file stream as an initial write before the actual buffer!!! 
    protobuf_c_message_pack_to_buffer(&message->proto.base, (ProtobufCBuffer*)&appender);

    return !appender.error;
}
