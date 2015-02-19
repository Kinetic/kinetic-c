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

#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic_memory.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include <stdlib.h>
#include <pthread.h>


KineticSession* KineticAllocator_NewSession(struct bus * b, KineticSessionConfig* config)
{
    (void)b; // TODO: combine session w/connection, which will use this variable
    
    // Allocate a new session
    KineticSession* session = KineticCalloc(1, sizeof(KineticSession));
    if (session == NULL) {
        LOG0("Failed allocating a new session!");
        return NULL;
    }

    // Copy the supplied config into the session config
    session->config = *config;
    // Update pointer to copy of key data
    session->config.hmacKey.data = session->config.keyData;
    strncpy(session->config.host, config->host, sizeof(session->config.host));

    return session;
}

void KineticAllocator_FreeSession(KineticSession* session)
{
    if (session != NULL) {
        KineticFree(session);
    }
}

KineticConnection* KineticAllocator_NewConnection(struct bus * b, KineticSession* const session)
{
    KineticConnection* connection = KineticCalloc(1, sizeof(KineticConnection));
    if (connection == NULL) {
        LOG0("Failed allocating new Connection!");
        return NULL;
    }
    KineticResourceWaiter_Init(&connection->connectionReady);
    connection->pSession = session;
    connection->messageBus = b;
    connection->socket = -1;  // start with an invalid file descriptor
    return connection;
}

void KineticAllocator_FreeConnection(KineticConnection* connection)
{
    KINETIC_ASSERT(connection != NULL);
    KineticResourceWaiter_Destroy(&connection->connectionReady);
    KineticFree(connection);
}

KineticResponse * KineticAllocator_NewKineticResponse(size_t const valueLength)
{
    KineticResponse * response = KineticCalloc(1, sizeof(*response) + valueLength);
    if (response == NULL) {
        LOG0("Failed allocating new response!");
        return NULL;
    }
    return response;
}

void KineticAllocator_FreeKineticResponse(KineticResponse * response)
{
    KINETIC_ASSERT(response != NULL);

    if (response->command != NULL) {
        protobuf_c_message_free_unpacked(&response->command->base, NULL);
    }
    if (response->proto != NULL) {
        protobuf_c_message_free_unpacked(&response->proto->base, NULL);
    }
    KineticFree(response);
}

KineticOperation* KineticAllocator_NewOperation(KineticConnection* const connection)
{
    KINETIC_ASSERT(connection != NULL);
    KINETIC_ASSERT(connection->pSession != NULL);
    LOGF3("Allocating new operation on connection (0x%0llX)", connection);
    KineticOperation* newOperation =
        (KineticOperation*)KineticCalloc(1, sizeof(KineticOperation));
    if (newOperation == NULL) {
        LOGF0("Failed allocating new operation on connection (0x%0llX)!", connection);
        return NULL;
    }
    KineticOperation_Init(newOperation, connection->pSession);
    LOGF3("Allocating new PDU on connection (0x%0llX)", connection);
    newOperation->request = (KineticPDU*)KineticCalloc(1, sizeof(KineticPDU));
    if (newOperation->request == NULL) {
        LOG0("Failed allocating new PDU!");
        KineticFree(newOperation);
        return NULL;
    }
    KineticPDU_InitWithCommand(newOperation->request, connection->pSession);
    LOGF3("Allocated new operation (0x%0llX) on connection (0x%0llX)", newOperation, connection);
    return newOperation;
}

void KineticAllocator_FreeOperation(KineticOperation* operation)
{
    KINETIC_ASSERT(operation != NULL);
    KineticConnection* const connection = operation->connection;
    LOGF3("Freeing operation (0x%0llX) on connection (0x%0llX)", operation, connection);
    if (operation->request != NULL) {
        LOGF3("Freeing request PDU (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
            operation->request, operation, connection);
        KineticFree(operation->request);
        LOGF3("Freed PDU (0x%0llX)", operation->request);
        operation->request = NULL;
    }
    if (operation->response != NULL) {
        LOGF3("Freeing response (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
            operation->response, operation, connection);
        KineticAllocator_FreeKineticResponse(operation->response);
        operation->response = NULL;
    }
    KineticFree(operation);
    LOGF3("Freed operation (0x%0llX) on connection (0x%0llX)", operation, connection);
}
