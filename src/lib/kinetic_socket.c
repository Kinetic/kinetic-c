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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "kinetic_types_internal.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include "socket99.h"

int KineticSocket_Connect(const char* host, int port, bool nonBlocking)
{
    char port_str[32];
    struct addrinfo hints;
    struct addrinfo* ai_result = NULL;
    struct addrinfo* ai = NULL;
    socket99_result result;

    // Setup server address info
    socket99_config cfg = {
        .host = (char*)host,
        .port = port,
        .nonblocking = nonBlocking,
    };
    sprintf(port_str, "%d", port);

    // Open socket
    LOGF0("Connecting to %s:%d", host, port);
    if (!socket99_open(&cfg, &result)) {
        LOGF0("Failed to open socket connection with host: status %d, errno %d",
             result.status, result.saved_errno);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }

    // Configure the socket
    socket99_set_hints(&cfg, &hints);
    if (getaddrinfo(cfg.host, port_str, &hints, &ai_result) != 0) {
        LOGF0("Failed to get socket address info: errno %d", errno);
        close(result.fd);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }

    for (ai = ai_result; ai != NULL; ai = ai->ai_next) {
        int setsockopt_result;
        int buffer_size = KINETIC_OBJ_SIZE;

#if defined(SO_NOSIGPIPE) && !defined(__APPLE__)
        // On BSD-like systems we can set SO_NOSIGPIPE on the socket to
        // prevent it from sending a PIPE signal and bringing down the whole
        // application if the server closes the socket forcibly
        int enable = 1;
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_NOSIGPIPE,
                                       &enable, sizeof(enable));
        // Allow ENOTSOCK because it allows tests to use pipes instead of
        // real sockets
        if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK) {
            LOG0("Failed to set SO_NOSIGPIPE on socket");
            continue;
        }
#endif

        // Increase send buffer to KINETIC_OBJ_SIZE
        // Note: OS allocates 2x this value for its overhead
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_SNDBUF,
                                       &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1) {
            LOG0("Error setting socket send buffer size");
            continue;
        }

        // Increase receive buffer to KINETIC_OBJ_SIZE
        // Note: OS allocates 2x this value for its overheadbuffer_size
        setsockopt_result = setsockopt(result.fd,
                                       SOL_SOCKET, SO_RCVBUF,
                                       &buffer_size, sizeof(buffer_size));
        if (setsockopt_result == -1) {
            LOG0("Error setting socket receive buffer size");
            continue;
        }

        break;
    }

    freeaddrinfo(ai_result);

    if (ai == NULL || result.fd == KINETIC_SOCKET_DESCRIPTOR_INVALID) {
        // we went through all addresses without finding one we could bind to
        LOGF0("Could not connect to %s:%d", host, port);
        return KINETIC_SOCKET_DESCRIPTOR_INVALID;
    }
    else {
        LOGF1("Successfully connected to %s:%d (fd=%d)", host, port, result.fd);
        return result.fd;
    }
}

void KineticSocket_Close(int socket)
{
    if (socket == -1) {
        LOG1("Not connected so no cleanup needed");
    }
    else {
        LOGF0("Closing socket with fd=%d", socket);
        if (close(socket) == 0) {
            LOG2("Socket closed successfully");
        }
        else {
            LOGF0("Error closing socket file descriptor!"
                 " (fd=%d, errno=%d, desc='%s')",
                 socket, errno, strerror(errno));
        }
    }
}

KineticWaitStatus KineticSocket_WaitUntilDataAvailable(int socket, int timeout)
{
    if (socket < 0) {return -1;}
    struct pollfd fd = {
        .fd = socket,
        .events = POLLIN,
        .revents = 0,
    };

    int res = poll(&fd, 1, timeout);

    if (res > 0) {
        //if (fd.revents & POLLHUP) // hung up
        if (fd.revents & POLLIN)
        {
            return KINETIC_WAIT_STATUS_DATA_AVAILABLE;
        }
        else
        {
            return KINETIC_WAIT_STATUS_FATAL_ERROR;
        }
    }
    else if (res == 0)
    {
        return KINETIC_WAIT_STATUS_TIMED_OUT;
    }
    else if ((errno & (EAGAIN | EINTR)) != 0)
    {
        return KINETIC_WAIT_STATUS_RETRYABLE_ERROR;
    }
    else
    {
        return KINETIC_WAIT_STATUS_FATAL_ERROR;
    }
}

int KineticSocket_DataBytesAvailable(int socket)
{
    if (socket < 0) {return -1;}
    int count = -1;
    ioctl(socket, FIONREAD, &count);
    return count;
}

KineticStatus KineticSocket_Read(int socket, ByteBuffer* dest, size_t len)
{
    LOGF2("Reading %zd bytes into buffer @ 0x%zX from fd=%d",
         len, (size_t)dest->array.data, socket);

    KineticStatus status = KINETIC_STATUS_INVALID;

    // Read "up to" the allocated number of bytes into dest buffer
    size_t bytesToReadIntoBuffer = len;
    if (dest->array.len < len) {
        bytesToReadIntoBuffer = dest->array.len;
    }
    while (dest->bytesUsed < bytesToReadIntoBuffer) {
        int opStatus;
        fd_set readSet;
        struct timeval timeout;

        // Time out after 5 seconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);
        opStatus = select(socket + 1, &readSet, NULL, NULL, &timeout);

        if (opStatus < 0) { // Error occurred
            LOGF0("Failed waiting to read from socket!"
                 " status=%d, errno=%d, desc='%s'",
                 opStatus, errno, strerror(errno));
            return KINETIC_STATUS_SOCKET_ERROR;
        }
        else if (opStatus == 0) { // Timeout occurred
            LOG0("Timed out waiting for socket data to arrive!");
            return KINETIC_STATUS_SOCKET_TIMEOUT;
        }
        else if (opStatus > 0) { // Data available to read
            // The socket is ready for reading
            opStatus = read(socket,
                            &dest->array.data[dest->bytesUsed],
                            dest->array.len - dest->bytesUsed);
            // Retry if no data yet...
            if (opStatus == -1 &&
                ((errno == EINTR) ||
                 (errno == EAGAIN) ||
                 (errno == EWOULDBLOCK)
                )) {
                continue;
            }
            else if (opStatus <= 0) {
                LOGF0("Failed to read from socket!"
                     " status=%d, errno=%d, desc='%s'",
                     opStatus, errno, strerror(errno));
                return KINETIC_STATUS_SOCKET_ERROR;
            }
            else {
                dest->bytesUsed += opStatus;
                LOGF3("Received %d bytes (%zd of %zd)",
                     opStatus, dest->bytesUsed, len);
            }
        }
    }

    // Flush any remaining data, in case of a truncated read w/short dest buffer
    if (dest->bytesUsed < len) {
        bool abortFlush = false;

        uint8_t* discardedBytes = malloc(len - dest->bytesUsed);
        if (discardedBytes == NULL) {
            LOG0("Failed allocating a socket read discard buffer!");
            abortFlush = true;
            status = KINETIC_STATUS_MEMORY_ERROR;
        }

        while (!abortFlush && dest->bytesUsed < len) {
            int opStatus;
            fd_set readSet;
            struct timeval timeout;
            size_t remainingLen = len - dest->bytesUsed;

            // Time out after 5 seconds
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            FD_ZERO(&readSet);
            FD_SET(socket, &readSet);
            opStatus = select(socket + 1, &readSet, NULL, NULL, &timeout);

            if (opStatus < 0) { // Error occurred
                LOGF0("Failure trying to flush read socket data!"
                     " status=%d, errno=%d, desc='%s'",
                     status, errno, strerror(errno));
                abortFlush = true;
                status = KINETIC_STATUS_SOCKET_ERROR;
                continue;
            }
            else if (opStatus == 0) { // Timeout occurred
                LOG0("Timed out waiting to flush socket data!");
                abortFlush = true;
                status = KINETIC_STATUS_SOCKET_TIMEOUT;
                continue;
            }
            else if (opStatus > 0) { // Data available to read
                // The socket is ready for reading
                opStatus = read(socket, discardedBytes, remainingLen);
                // Retry if no data yet...
                if (opStatus == -1 &&
                    ((errno == EINTR) ||
                     (errno == EAGAIN) ||
                     (errno == EWOULDBLOCK)
                    )) {
                    continue;
                }
                else if (opStatus <= 0) {
                    LOGF0("Failed to read from socket while flushing!"
                         " status=%d, errno=%d, desc='%s'",
                         opStatus, errno, strerror(errno));
                    abortFlush = true;
                    status = KINETIC_STATUS_SOCKET_ERROR;
                }
                else {
                    dest->bytesUsed += opStatus;
                    LOGF3("Flushed %d bytes from socket read pipe (%zd of %zd)",
                         opStatus, dest->bytesUsed, len);
                }
            }
        }

        // Free up dynamically allocated memory before returning
        if (discardedBytes != NULL) {
            free(discardedBytes);
        }

        // Report any error that occurred during socket flush
        if (abortFlush) {
            LOG0("Socket read pipe flush aborted!");
            assert(status == KINETIC_STATUS_SUCCESS);
            return status;
        }

        // Report truncation of data for any variable length byte arrays
        LOGF1("Socket read buffer was truncated due to buffer overrun!"
             " received=%zu, copied=%zu",
             len, dest->array.len);
        return KINETIC_STATUS_BUFFER_OVERRUN;
    }
    LOGF3("Received %zd of %zd bytes requested", dest->bytesUsed, len);
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSocket_ReadProtobuf(int socket, KineticPDU* pdu)
{
    size_t bytesToRead = pdu->header.protobufLength;
    LOGF2("Reading %zd bytes of protobuf", bytesToRead);

    uint8_t* packed = (uint8_t*)malloc(bytesToRead);
    if (packed == NULL) {
        LOG0("Failed allocating memory for protocol buffer");
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    ByteBuffer recvBuffer = ByteBuffer_Create(packed, bytesToRead, 0);
    KineticStatus status = KineticSocket_Read(socket, &recvBuffer, bytesToRead);

    if (status != KINETIC_STATUS_SUCCESS) {
        LOG0("Protobuf read failed!");
        free(packed);
        return status;
    }
    else {
        pdu->proto = KineticProto_Message__unpack(
                         NULL, recvBuffer.bytesUsed, recvBuffer.array.data);
    }

    free(packed);

    if (pdu->proto == NULL) {
        pdu->protobufDynamicallyExtracted = false;
        LOG0("Error unpacking incoming Kinetic protobuf message!");
        return KINETIC_STATUS_DATA_ERROR;
    }
    else {
        pdu->protobufDynamicallyExtracted = true;
        LOG3("Protobuf unpacked successfully!");
        return KINETIC_STATUS_SUCCESS;
    }
}

KineticStatus KineticSocket_Write(int socket, ByteBuffer* src)
{
    LOGF3("Writing %zu bytes to socket...", src->bytesUsed);
    for (unsigned int bytesSent = 0; bytesSent < src->bytesUsed;) {
        int bytesRemaining = src->bytesUsed - bytesSent;
        int status = write(socket, &src->array.data[bytesSent], bytesRemaining);
        if (status == -1 &&
            ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))) {
            LOG2("Write interrupted. retrying...");
            continue;
        }
        else if (status <= 0) {
            LOGF0("Failed to write to socket! status=%d, errno=%d\n", status, errno);
            return KINETIC_STATUS_SOCKET_ERROR;
        }
        else {
            bytesSent += status;
            LOGF2("Wrote %d bytes (%d of %zu sent)", status, bytesSent, src->bytesUsed);
        }
    }
    LOG3("Socket write completed successfully");
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSocket_WriteProtobuf(int socket, KineticPDU* pdu)
{
    assert(pdu != NULL);
    LOGF1("Writing protobuf (%zd bytes)...", pdu->header.protobufLength);

    uint8_t* packed = (uint8_t*)malloc(pdu->header.protobufLength);

    if (packed == NULL) {
        LOG0("Failed allocating memory for protocol buffer");
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    size_t len = KineticProto_Message__pack(&pdu->protoData.message.message, packed);
    assert(len == pdu->header.protobufLength);

    ByteBuffer buffer = ByteBuffer_Create(packed, len, len);

    KineticStatus status = KineticSocket_Write(socket, &buffer);

    free(packed);
    return status;
}
