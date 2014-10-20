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

// #define KINETIC_LOG_ALLOCATOR

static inline void KineticAllocator_LockList(KineticList* const list)
{
    assert(!list->locked);
    pthread_mutex_lock(&list->mutex);
    list->locked = true;
}

static inline void KineticAllocator_UnlockList(KineticList* const list)
{
    // assert(list->locked);
    pthread_mutex_unlock(&list->mutex);
    list->locked = false;
}

void KineticAllocator_InitList(KineticList* const list)
{
    *list = (KineticList) {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .locked = false,
    };
}

void* KineticAllocator_NewItem(KineticList* const list, size_t size)
{
    KineticListItem* newItem = (KineticListItem*)malloc(sizeof(KineticListItem));
    if (newItem == NULL) {
        LOG("Failed allocating new list item!");
        return NULL;
    }
    memset(newItem, 0, sizeof(KineticListItem));
    newItem->data = malloc(size);
    if (newItem->data == NULL) {
        LOG("Failed allocating data for list item!");
        return NULL;
    }
    memset(newItem->data, 0, size);

    // Add the new item to the list
    KineticAllocator_LockList(list);
    if (list->start == NULL) {
        list->start = newItem;
    }
    else {
        newItem->previous = list->last;
        list->last->next = newItem;
    }
    list->last = newItem;
    KineticAllocator_UnlockList(list);

    #ifdef KINETIC_LOG_ALLOCATOR
    LOGF("Allocated new list item @ 0x%0llX w/data @ 0x%0llX",
         (long long)newItem, (long long)newItem->data);
    #endif

    return newItem->data;
}

void KineticAllocator_FreeItem(KineticList* const list, void* item)
{
    KineticAllocator_LockList(list);
    KineticListItem* cur = list->start;
    while (cur->data != item) {
        if (cur->next == NULL) {
            LOG("  Reached end of list before finding item to free!");
            KineticAllocator_UnlockList(list);
            return;
        }
        else {
            cur = cur->next;
        }
    }
    // LOG("  Done searching for item list item");

    if ((cur != NULL) && (cur->data == item)) {
        // LOG("  item found! freeing it.");

        // Handle PDU list emptied
        if (cur->previous == NULL) {
            // LOG("  At start of list.");
            if (cur->next == NULL) {
                // LOG("  Making it empty, since all deallocated!");
                list->start = NULL;
                list->last = NULL;
            }
            else {
                // LOG("  Moving current item to head, since head deallocated!");
                list->start = cur->next;
                list->start->previous = NULL;
            }
        }
        else {
            // Relink from previous to next, if avaliable
            // LOG("  Not at list start, so relinking list to free item.");
            if (cur->previous->next != NULL) {
                // LOG("  Relinking previous to next");
                if (cur->next != NULL) {
                    // LOG("    next being reset!");
                    cur->previous->next = cur->next;
                }
                else {
                    list->last = cur->previous;
                    list->last->next = NULL;
                    // LOGF("    next is NULL. End of list now @ 0x%0llX",
                    //      (long long)list->last);
                }
            }
            else {
                LOG("  This shouldn't happen!");
                list->last = cur->previous;
            }
        }

        #ifdef KINETIC_LOG_ALLOCATOR
        LOGF("  Freeing item @ 0x%0llX, item @ 0x%0llX",
             (long long)cur, (long long)&cur->data);
        #endif
        free(cur->data);
        cur->data = NULL;
        free(cur);
        cur = NULL;
    }
    KineticAllocator_UnlockList(list);
}

void KineticAllocator_FreeList(KineticList* const list)
{
    if (list != NULL) {
        #ifdef KINETIC_LOG_ALLOCATOR
        LOG("Freeing list of all items");
        #endif

        KineticAllocator_LockList(list);

        KineticListItem* current = list->start;

        while (current->next != NULL) {
            #ifdef KINETIC_LOG_ALLOCATOR
            LOG("Advancing to next list item...");
            #endif
            current = current->next;
        }

        while (current != NULL) {
            #ifdef KINETIC_LOG_ALLOCATOR
            LOG("  Current item not freed!");
            LOGF("  DEALLOCATING item: 0x%0llX, data: 0x%llX, prev: 0x%0llX",
                (long long)current, (long long)&current->data, (long long)current->previous);
            #endif
            KineticListItem* curItem = current;
            KineticListItem* prevItem = current->previous;
            if (curItem != NULL) {
                #ifdef KINETIC_LOG_ALLOCATOR
                LOG("  Freeing list item");
                #endif
                if (curItem->data != NULL) {
                    free(curItem->data);
                }
                free(curItem);
            }
            current = prevItem;
            #ifdef KINETIC_LOG_ALLOCATOR
            LOGF("  on to prev=0x%llX", (long long)current);
            #endif
        }

        *list = (KineticList) {
            .start = NULL, .last = NULL
        };

        KineticAllocator_UnlockList(list);
    }
    else {
        #ifdef KINETIC_LOG_ALLOCATOR
        LOG("  Nothing to free!");
        #endif
    }

}

KineticPDU* KineticAllocator_NewPDU(KineticList* const list, KineticConnection* connection)
{
    assert(connection != NULL);
    KineticPDU* newPDU = (KineticPDU*)KineticAllocator_NewItem(
                             list, sizeof(KineticPDU));
    if (newPDU == NULL) {
        LOG("Failed allocating new PDU!");
        return NULL;
    }
    assert(newPDU->proto == NULL);
    KINETIC_PDU_INIT(newPDU, connection);
    #ifdef KINETIC_LOG_ALLOCATOR
    LOGF("Allocated new PDU @ 0x%0llX", (long long)newPDU);
    #endif
    return newPDU;
}

void KineticAllocator_FreePDU(KineticList* const list, KineticPDU* pdu)
{
    KineticAllocator_LockList(list);
    if ((pdu->proto != NULL) && pdu->protobufDynamicallyExtracted) {
        #ifdef KINETIC_LOG_ALLOCATOR
        LOG("Freeing dynamically allocated protobuf");
        #endif
        KineticProto_Message__free_unpacked(pdu->proto, NULL);
    };
    KineticAllocator_UnlockList(list);
    KineticAllocator_FreeItem(list, (void*)pdu);
}

KineticPDU* KineticAllocator_GetFirstPDU(KineticList* const list)
{
    assert(list != NULL);
    if (list->start == NULL) {
        return NULL;
    }
    return (KineticPDU*)list->start->data;
}

KineticPDU* KineticAllocator_GetNextPDU(KineticList* const list, KineticPDU* pdu)
{
    assert(list != NULL);
    KineticPDU* next = NULL;

    KineticAllocator_LockList(list);
    KineticListItem* current = list->start;
    while (current != NULL) {
        KineticPDU* currPDU = (KineticPDU*)current->data;
        if (currPDU == pdu) {
            if (current->next != NULL) {
                next = (KineticPDU*)current->next->data;
            }
            break;
        }
        current = current->next;
    }
    KineticAllocator_UnlockList(list);
    
    return next;
}

void KineticAllocator_FreeAllPDUs(KineticList* const list)
{
    if (list->start != NULL) {
        LOG("Freeing all PDUs...");
        KineticAllocator_LockList(list);
        KineticListItem* current = list->start;
        while (current != NULL) {
            KineticPDU* pdu = (KineticPDU*)current->data;
            if (pdu != NULL && pdu->proto != NULL
                && pdu->protobufDynamicallyExtracted) {
                KineticProto_Message__free_unpacked(pdu->proto, NULL);
            }
            current = current->next;
        }
        KineticAllocator_UnlockList(list);
        KineticAllocator_FreeList(list);
    }
    else {
        #ifdef KINETIC_LOG_ALLOCATOR
        LOG("  Nothing to free!");
        #endif
    }
}

bool KineticAllocator_ValidateAllMemoryFreed(KineticList* const list)
{
    bool empty = (list->start == NULL);
    #ifdef KINETIC_LOG_ALLOCATOR
    LOGF("  PDUList: 0x%0llX, empty=%s",
        (long long)list->start, empty ? "true" : "false");
    #endif
    return empty;
}
