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
#include <stdlib.h>
#include <pthread.h>


//==============================================================================
// Generic List Support (INTERNAL)
//==============================================================================

#define KINETIC_LIST_LOCK(_list) { \
    /*LOG_LOCATION; LOGF3("Locking list! (list_addr=0x%llX)", (_list));*/ \
    pthread_mutex_lock(&((_list)->mutex)); \
    ((_list)->locked) = true; \
}

#define KINETIC_LIST_UNLOCK(_list) { \
    /*LOG_LOCATION; LOGF3("Unlocking list! (list_addr=0x%llX)", (_list));*/ \
    assert(((_list)->locked)); \
    pthread_mutex_unlock(&((_list)->mutex)); \
    ((_list)->locked) = false; \
}

void KineticAllocator_InitLists(KineticConnection* connection)
{
    assert(connection != NULL);
    connection->pdus = KINETIC_LIST_INITIALIZER;
    connection->operations = KINETIC_LIST_INITIALIZER;
}

KineticConnection* KineticAllocator_NewConnection(void)
{
    KineticConnection* connection = calloc(1, sizeof(KineticConnection));
    KINETIC_CONNECTION_INIT(connection);
    return connection;
}

void KineticAllocator_FreeConnection(KineticConnection* connection)
{
    assert(connection != NULL);
    KineticAllocator_FreeAll(connection);
    free(connection);
}

static void* KineticAllocator_GetFirstListItem(KineticList* list)
{
    assert(list != NULL);
    if (list->start == NULL) {
        return NULL;
    }
    return list->start->data;
}

static void* KineticAllocator_GetNextListItem(KineticList* list, void* item_data)
{
    assert(list != NULL);
    void* nextData = NULL;
    KINETIC_LIST_LOCK(list);
    KineticListItem* current = list->start;
    while (current != NULL) {
        void* currData = current->data;
        if (currData == item_data) {
            if (current->next != NULL) {
                nextData = current->next->data;
            }
            break;
        }
        current = current->next;
    }
    KINETIC_LIST_UNLOCK(list);
    return nextData;
}

static void* KineticAllocator_NewItem(KineticList* const list, size_t size)
{
    KineticListItem* newItem = (KineticListItem*)malloc(sizeof(KineticListItem));
    if (newItem == NULL) {
        LOG0("  Failed allocating new list item!");
        return NULL;
    }
    memset(newItem, 0, sizeof(KineticListItem));
    newItem->data = malloc(size);
    if (newItem->data == NULL) {
        LOG0("  Failed allocating data for list item!");
        return NULL;
    }
    memset(newItem->data, 0, size);

    // Add the new item to the list
    KINETIC_LIST_LOCK(list);
    newItem->next = list->start;
    list->start = newItem;
    KINETIC_LIST_UNLOCK(list);

    LOGF3("  Allocated new list item (0x%0llX) w/data (0x%0llX)",
         (long long)newItem, (long long)newItem->data);

    return newItem->data;
}

static void KineticAllocator_FreeItem(KineticList* const list, void* item, bool lock)
{
    /* Make locking optional, since the lock may already be owned by the caller. */
    if (lock) {
        KINETIC_LIST_LOCK(list);
    }
    KineticListItem* cur = list->start;
    KineticListItem* prev = NULL;

    while (cur->data != item) {
        if (cur->next == NULL) {
            LOG1("  Reached end of list before finding item to free!");
            if (lock) {
                KINETIC_LIST_UNLOCK(list);
            }
            return;
        }
        else {
            prev = cur;
            cur = cur->next;
        }
    }
    LOG3("  Done searching for item list item");

    if ((cur != NULL) && (cur->data == item)) {
        LOG3("  item found! freeing it.");

        if (prev == NULL) {
            LOG3("  At start of list.");
            list->start = cur->next;
        } else {
            LOG3("  Not at list start, so relinking list to free item.");
            prev->next = cur->next;
        }

        LOGF3("  Freeing item (0x%0llX) w/data (0x%0llX)", cur, &cur->data);
        free(cur->data);
        cur->data = NULL;
        free(cur);
        cur = NULL;
    }
    if (lock) {
        KINETIC_LIST_UNLOCK(list);
    }
}

static void KineticAllocator_FreeList(KineticList* const list)
{
    if (list != NULL) {
        LOGF3("  Freeing list (0x%0llX) of all items...", list);
        KINETIC_LIST_LOCK(list);

        KineticListItem* next = NULL;
        for (KineticListItem* item = list->start; item; item = next) {
            next = item->next;
            
            LOGF3("  Freeing list item (0x%0llX) w/ data (0x%llX)",
                    (long long)item, (long long)&item->data);
                if (item->data != NULL) {
                    free(item->data);
                    item->data = NULL;
                }
                free(item);
        }

        // Make list empty, but leave mutex alone so the state is retained!
        list->start = NULL;
        KINETIC_LIST_UNLOCK(list);
    }
    else {
        LOGF3("  Nothing to free from list (0x%0llX)", list);
    }
}


//==============================================================================
// PDU List Support
//==============================================================================

KineticPDU* KineticAllocator_NewPDU(KineticConnection* connection)
{
    assert(connection != NULL);
    LOGF3("Allocating new PDU on connection (0x%0llX)", connection);
    KineticPDU* newPDU = (KineticPDU*)KineticAllocator_NewItem(
                             &connection->pdus, sizeof(KineticPDU));
    if (newPDU == NULL) {
        LOG0("Failed allocating new PDU!");
        return NULL;
    }
    assert(newPDU->proto == NULL);
    KINETIC_PDU_INIT(newPDU, connection);
    LOGF3("Allocated new PDU (0x%0llX) on connection", newPDU, connection);
    return newPDU;
}

void KineticAllocator_FreePDU(KineticConnection* connection, KineticPDU* pdu)
{
    LOGF3("Freeing PDU (0x%0llX) on connection (0x%0llX)", pdu, connection);
    KINETIC_LIST_LOCK(&connection->pdus);
    if (pdu && (pdu->proto != NULL) && pdu->protobufDynamicallyExtracted) {
        LOG3("Freeing dynamically allocated protobuf");
        KineticProto_Message__free_unpacked(pdu->proto, NULL);
        pdu->proto = NULL;
    };
    
    /* TODO: We can't unlock until the function below completes, but it
     *     normally also tries to lock, so pass in a flag indicating
     *     we already have it locked. The way that the mutexes are
     *     currently initialized makes adding an attribute of
     *     PTHREAD_MUTEX_RECURSIVE significantly more trouble. */
    KineticAllocator_FreeItem(&connection->pdus, (void*)pdu, false);
    KINETIC_LIST_UNLOCK(&connection->pdus);
    LOGF3("Freed PDU (0x%0llX) on connection (0x%0llX)", pdu, connection);
}

KineticPDU* KineticAllocator_GetFirstPDU(KineticConnection* connection)
{
    assert(connection != NULL);
    return (KineticPDU*)KineticAllocator_GetFirstListItem(&connection->pdus);
}

KineticPDU* KineticAllocator_GetNextPDU(KineticConnection* connection, KineticPDU* pdu)
{
    assert(connection != NULL);
    return (KineticPDU*)KineticAllocator_GetNextListItem(&connection->pdus, pdu);
}


//==============================================================================
// Operation List Support
//==============================================================================

KineticOperation* KineticAllocator_NewOperation(KineticConnection* const connection)
{
    assert(connection != NULL);
    LOGF3("Allocating new operation on connection (0x%0llX)", connection);
    KineticOperation* newOperation =
        (KineticOperation*)KineticAllocator_NewItem(&connection->operations, sizeof(KineticOperation));
    if (newOperation == NULL) {
        LOGF0("Failed allocating new operation on connection (0x%0llX)!", connection);
        return NULL;
    }
    KINETIC_OPERATION_INIT(newOperation, connection);
    newOperation->request = KineticAllocator_NewPDU(connection);
    KINETIC_PDU_INIT_WITH_COMMAND(newOperation->request, connection, connection->session->config.clusterVersion);
    LOGF3("Allocated new operation (0x%0llX) on connection (0x%0llX)", newOperation, connection);
    return newOperation;
}

void KineticAllocator_FreeOperation(KineticConnection* const connection, KineticOperation* operation)
{
    assert(connection != NULL);
    assert(operation != NULL);
    LOGF3("Freeing operation (0x%0llX) on connection (0x%0llX)", operation, connection);
    if (operation->request != NULL) {
        LOGF3("Freeing request PDU (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
            operation->request, operation, connection);
        KineticAllocator_FreePDU(connection, operation->request);
        operation->request = NULL;
    }
    if (operation->response != NULL) {
        LOGF3("Freeing response PDU (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
            operation->response, operation, connection);
        KineticAllocator_FreePDU(connection, operation->response);
        operation->response = NULL;
    }
    pthread_mutex_destroy(&operation->timeoutTimeMutex);
    KineticAllocator_FreeItem(&connection->operations, (void*)operation, true);
    LOGF3("Freed operation (0x%0llX) on connection (0x%0llX)", operation, connection);
}

KineticOperation* KineticAllocator_GetFirstOperation(KineticConnection* const connection)
{
    assert(connection != NULL);
    return (KineticOperation*)KineticAllocator_GetFirstListItem(&connection->operations);
}

KineticOperation* KineticAllocator_GetNextOperation(KineticConnection* const connection, KineticOperation* operation)
{
    assert(connection != NULL);
    return (KineticOperation*)KineticAllocator_GetNextListItem(&connection->operations, operation);
}

void KineticAllocator_FreeAll(KineticConnection* const connection)
{
    assert(connection != NULL);

    KineticOperation* op = KineticAllocator_GetFirstOperation(connection);
    while (op) {
        KineticAllocator_FreeOperation(connection, op);
        op = KineticAllocator_GetFirstOperation(connection);
    }

    if (connection->pdus.start != NULL) {
        LOG3("Freeing all PDUs...");
        KINETIC_LIST_LOCK(&connection->pdus);
        KineticListItem* current = connection->pdus.start;
        while (current != NULL) {
            KineticPDU* pdu = (KineticPDU*)current->data;
            if (pdu != NULL && pdu->proto != NULL
                && pdu->protobufDynamicallyExtracted) {
                KineticProto_Message__free_unpacked(pdu->proto, NULL);
            }
            current = current->next;
        }
        KINETIC_LIST_UNLOCK(&connection->pdus);
        KineticAllocator_FreeList(&connection->pdus);
    }
}

bool KineticAllocator_ValidateAllMemoryFreed(KineticConnection* const connection)
{
    assert(connection != NULL);
    LOGF3("Checking to see if all memory has been freed from connection (0x%0llX)...",
        connection);
    bool empty = true;
    LOGF3("  Operations: 0x%0llX, empty=%s",connection->operations.start,
        BOOL_TO_STRING(connection->operations.start == NULL));
    if (connection->operations.start != NULL) {empty = false;}
    LOGF3("  PDUs: 0x%0llX, empty=%s", connection->pdus.start,
        BOOL_TO_STRING(connection->pdus.start == NULL));
    if (connection->pdus.start != NULL) {empty = false;}
    return empty;
}
