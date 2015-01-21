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
#include <stdlib.h>
#include <pthread.h>

KineticConnection* KineticAllocator_NewConnection(void)
{
    KineticConnection* connection = KineticCalloc(1, sizeof(KineticConnection));
    if (connection == NULL) {
        LOG0("Failed allocating new Connection!");
        return NULL;
    }
    return connection;
}

void KineticAllocator_FreeConnection(KineticConnection* connection)
{
    assert(connection != NULL);
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
    assert(response != NULL);

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
    assert(connection != NULL);
    LOGF3("Allocating new operation on connection (0x%0llX)", connection);
    KineticOperation* newOperation =
        (KineticOperation*)KineticCalloc(1, sizeof(KineticOperation));
    if (newOperation == NULL) {
        LOGF0("Failed allocating new operation on connection (0x%0llX)!", connection);
        return NULL;
    }
    KineticOperation_Init(newOperation, connection);
    LOGF3("Allocating new PDU on connection (0x%0llX)", connection);
    newOperation->request = (KineticPDU*)KineticCalloc(1, sizeof(KineticPDU));
    if (newOperation->request == NULL) {
        LOG0("Failed allocating new PDU!");
        KineticFree(newOperation);
        return NULL;
    }
    KineticPDU_InitWithCommand(newOperation->request, connection);
    LOGF3("Allocated new operation (0x%0llX) on connection (0x%0llX)", newOperation, connection);
    return newOperation;
}

void KineticAllocator_FreeOperation(KineticOperation* operation)
{
    assert(operation != NULL);
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
