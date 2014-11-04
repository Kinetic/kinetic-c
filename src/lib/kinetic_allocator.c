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
    assert(!((_list)->locked)); \
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
    if (list->start == NULL) {
        list->start = newItem;
    }
    else {
        newItem->previous = list->last;
        list->last->next = newItem;
    }
    list->last = newItem;
    KINETIC_LIST_UNLOCK(list);

    LOGF3("  Allocated new list item (0x%0llX) w/data (0x%0llX)",
         (long long)newItem, (long long)newItem->data);

    return newItem->data;
}

static void KineticAllocator_FreeItem(KineticList* const list, void* item)
{
    KINETIC_LIST_LOCK(list);
    KineticListItem* cur = list->start;
    while (cur->data != item) {
        if (cur->next == NULL) {
            LOG1("  Reached end of list before finding item to free!");
            KINETIC_LIST_UNLOCK(list);
            return;
        }
        else {
            cur = cur->next;
        }
    }
    LOG3("  Done searching for item list item");

    if ((cur != NULL) && (cur->data == item)) {
        LOG3("  item found! freeing it.");

        // Handle PDU list emptied
        if (cur->previous == NULL) {
            LOG3("  At start of list.");
            if (cur->next == NULL) {
                LOG3("  Making it empty, since all deallocated!");
                list->start = NULL;
                list->last = NULL;
            }
            else {
                LOG3("  Moving current item to head, since head deallocated!");
                list->start = cur->next;
                list->start->previous = NULL;
            }
        }
        else {
            // Relink from previous to next, if avaliable
            LOG3("  Not at list start, so relinking list to free item.");
            if (cur->previous->next != NULL) {
                LOG3("  Relinking previous to next");
                if (cur->next != NULL) {
                    LOG3("    Next being reset!");
                    cur->previous->next = cur->next;
                }
                else {
                    list->last = cur->previous;
                    list->last->next = NULL;
                    LOGF3("    Next is NULL. End of list now @ 0x%0llX",
                         (long long)list->last);
                }
            }
            else {
                LOG1("  This shouldn't happen!");
                list->last = cur->previous;
            }
        }

        LOGF3("  Freeing item (0x%0llX) w/data (0x%0llX)", cur, &cur->data);
        free(cur->data);
        cur->data = NULL;
        free(cur);
        cur = NULL;
    }
    KINETIC_LIST_UNLOCK(list);
}

static void KineticAllocator_FreeList(KineticList* const list)
{
    if (list != NULL) {
        LOGF3("  Freeing list (0x%0llX) of all items...", list);
        KINETIC_LIST_LOCK(list);
        KineticListItem* current = list->start;

        while (current->next != NULL) {
            LOG3("  Advancing to next list item...");
            current = current->next;
        }

        while (current != NULL) {
            KineticListItem* curItem = current;
            KineticListItem* prevItem = current->previous;
            if (curItem != NULL) {
                LOGF3("  Freeing list item (0x%0llX) w/ data (0x%llX)",
                    (long long)current,
                    (long long)&current->data,
                    (long long)current->previous);
                if (curItem->data != NULL) {
                    free(curItem->data);
                }
                free(curItem);
            }
            current = prevItem;
            LOGF3("  on to previous list item (0x%llX)...", current);
        }

        // Make list empty, but leave mutex alone so the state is retained!
        list->start = NULL;
        list->last = NULL;
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
    if ((pdu->proto != NULL) && pdu->protobufDynamicallyExtracted) {
        LOG3("Freeing dynamically allocated protobuf");
        KineticProto_Message__free_unpacked(pdu->proto, NULL);
    };
    KINETIC_LIST_UNLOCK(&connection->pdus);
    KineticAllocator_FreeItem(&connection->pdus, (void*)pdu);
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

void KineticAllocator_FreeAllPDUs(KineticConnection* connection)
{
    assert(connection != NULL);
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
    else {
        LOG1("  Nothing to free!");
    }
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
    KINETIC_PDU_INIT_WITH_COMMAND(newOperation->request, connection);
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
    }
    if (operation->response != NULL) {
        LOGF3("Freeing response PDU (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
            operation->response, operation, connection);
        KineticAllocator_FreePDU(connection, operation->response);
    }
    KineticAllocator_FreeItem(&connection->operations, (void*)operation);
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

void KineticAllocator_FreeAllOperations(KineticConnection* const connection)
{
    assert(connection != NULL);
    KineticListItem* current = connection->operations.start;
    if (current != NULL) {
        LOGF3("Freeing operations list (0x%0llX) from connection (0x%0llX)...",
            &connection->operations, connection);
        while (current != NULL) {
            KineticOperation* op = (KineticOperation*)current->data;
            if (op != NULL) {

                if (op->request != NULL) {
                    LOGF3("Freeing request PDU (0x%0llX) from operation (0x%0llX) on connection (0x%0llX)",
                        op->request, op, connection);
                    KineticAllocator_FreePDU(connection, op->request);
                }

                if (op->response != NULL) {
                    LOGF3("Freeing response PDU (0x%0llX) from op (0x%0llX) on connection (0x%0llX)",
                        op->response, op, connection);
                    KineticAllocator_FreePDU(connection, op->response);
                }

                current = current->next;
                KineticAllocator_FreeItem(&connection->operations, (void*)op);
            }
        }
    }
    else {
        LOG1("  Nothing to free!");
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
