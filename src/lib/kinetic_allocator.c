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

pthread_mutex_t _global_pdu_lists_mutex = PTHREAD_MUTEX_INITIALIZER;
STATIC bool listsLocked = false;

static inline void KineticAllocator_Lock(void)
{
    pthread_mutex_lock(&_global_pdu_lists_mutex);
    assert(!listsLocked);
    listsLocked = true;
}

static inline void KineticAllocator_Unlock(void)
{
    pthread_mutex_unlock(&_global_pdu_lists_mutex);
    listsLocked = false;
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
    memset(newItem->data, 0, size);

    // Add the new item to the list
    KineticAllocator_Lock();
    if (list->start == NULL) {
        list->start = newItem;
    }
    else {
        newItem->previous = list->last;
        list->last->next = newItem;
    }
    list->last = newItem;
    KineticAllocator_Unlock();

    // LOGF("Allocated new list item @ 0x%0llX w/data @ 0x%0llX",
    //      (long long)newItem, (long long)&newItem->data);

    return newItem->data;
}

void KineticAllocator_FreeItem(KineticList* const list, void* item)
{
    KineticAllocator_Lock();
    KineticListItem* cur = list->start;
    while (cur->data != item) {
        if (cur->next == NULL) {
            LOG("  Reached end of list before finding item to free!");
            KineticAllocator_Unlock();
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

        // LOGF("  Freeing item @ 0x%0llX, item @ 0x%0llX",
        //      (long long)cur, (long long)&cur->data);
        free(cur->data);
        cur->data = NULL;
        free(cur);
        cur = NULL;
    }
    KineticAllocator_Unlock();
}

void KineticAllocator_FreeList(KineticList* const list)
{
    if (list != NULL) {
        LOG("Freeing list of all items");

        KineticAllocator_Lock();

        KineticListItem* current = list->start;

        while (current->next != NULL) {
            // LOG("Advancing to next list item...");
            current = current->next;
        }

        while (current != NULL) {
            // LOG("  Current item not freed!");
            // LOGF("  DEALLOCATING item: 0x%0llX, data: 0x%llX, prev: 0x%0llX",
            //     (long long)current, (long long)&current->data, (long long)current->previous);
            KineticListItem* curItem = current;
            KineticListItem* prevItem = current->previous;
            if (curItem != NULL) {
                // LOG("  Freeing list item");
                if (curItem->data != NULL) {
                    free(curItem->data);
                }
                free(curItem);
            }
            current = prevItem;
            // LOGF("  on to prev=0x%llX", (long long)current);
        }

        *list = (KineticList) {
            .start = NULL, .last = NULL
        };

        KineticAllocator_Unlock();
    }
    else {
        LOG("  Nothing to free!");
    }

}



KineticPDU* KineticAllocator_NewPDU(KineticList* const list)
{
    KineticPDU* newPDU = (KineticPDU*)KineticAllocator_NewItem(
                             list, sizeof(KineticPDU));
    if (newPDU == NULL) {
        LOG("Failed allocating new PDU!");
        return NULL;
    }
    assert(newPDU->proto == NULL);
    // LOGF("Allocated new PDU @ 0x%0llX", (long long)newPDU);
    return newPDU;
}

void KineticAllocator_FreePDU(KineticList* const list, KineticPDU* pdu)
{
    KineticAllocator_Lock();
    if ((pdu->proto != NULL) && pdu->protobufDynamicallyExtracted) {
        // LOG("Freeing dynamically allocated protobuf");
        KineticProto_Message__free_unpacked(pdu->proto, NULL);
    };
    KineticAllocator_Unlock();
    KineticAllocator_FreeItem(list, (void*)pdu);
}

void KineticAllocator_FreeAllPDUs(KineticList* const list)
{
    if (list->start != NULL) {
        LOG("Freeing all PDUs...");
        KineticAllocator_Lock();
        KineticListItem* current = list->start;
        while (current != NULL) {
            KineticPDU* pdu = (KineticPDU*)current->data;
            if (pdu != NULL && pdu->proto != NULL
                && pdu->protobufDynamicallyExtracted) {
                KineticProto_Message__free_unpacked(pdu->proto, NULL);
            }
            current = current->next;
        }
        KineticAllocator_Unlock();
        KineticAllocator_FreeList(list);
    }
    else {
        LOG("  Nothing to free!");
    }
}

bool KineticAllocator_ValidateAllMemoryFreed(KineticList* const list)
{
    bool empty = (list->start == NULL);
    // LOGF("  PDUList: 0x%0llX, empty=%s",
    // (long long)list->start, empty ? "true" : "false");
    return empty;
}
