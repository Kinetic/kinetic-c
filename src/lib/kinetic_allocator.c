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

STATIC KineticList PDUList = {.start = NULL, .last = NULL};

void* KineticAllocator_NewItem(KineticList* list, size_t size)
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
    if (list->start == NULL) {
        list->start = newItem;
    }
    else {
        newItem->previous = list->last;
        list->last->next = newItem;
    }
    list->last = newItem;

    // LOGF("Allocated new list item @ 0x%0llX w/data @ 0x%0llX",
    //      (long long)newItem, (long long)&newItem->data);

    return newItem->data;
}

void KineticAllocator_FreeItem(KineticList* list, void* item)
{
    KineticListItem* cur = list->start;
    while (cur->data != item) {
        if (cur->next == NULL) {
            LOG("  Reached end of list before finding item to free!");
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
}

void KineticAllocator_FreeList(KineticList* list)
{
    LOG_LOCATION;
    if (list != NULL) {
        LOG("Freeing list of all items");
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
    }
    else {
        LOG("  Nothing to free!");
    }

    *list = (KineticList) {
        .start = NULL, .last = NULL
    };
}



KineticPDU* KineticAllocator_NewPDU(void)
{
    KineticPDU* newPDU = (KineticPDU*)KineticAllocator_NewItem(
                             &PDUList, sizeof(KineticPDU));
    if (newPDU == NULL) {
        LOG("Failed allocating new PDU!");
        return NULL;
    }
    assert(newPDU->proto == NULL);
    // LOGF("Allocated new PDU @ 0x%0llX", (long long)newPDU);
    return newPDU;
}

void KineticAllocator_FreePDU(KineticPDU* pdu)
{
    if ((pdu->proto != NULL) && pdu->protobufDynamicallyExtracted) {
        // LOG("Freeing dynamically allocated protobuf");
        KineticProto__free_unpacked(pdu->proto, NULL);
    };
    KineticAllocator_FreeItem(&PDUList, (void*)pdu);
}

void KineticAllocator_FreeAllPDUs(void)
{
    LOG_LOCATION;
    if (PDUList.start != NULL) {
        LOG("Freeing all PDUs...");
        KineticListItem* current = PDUList.start;
        while (current != NULL) {
            KineticPDU* pdu = (KineticPDU*)current->data;
            if (pdu != NULL && pdu->proto != NULL
                && pdu->protobufDynamicallyExtracted) {
                KineticProto__free_unpacked(pdu->proto, NULL);
            }
            current = current->next;
        }
        KineticAllocator_FreeList(&PDUList);
    }
    else {
        LOG("  Nothing to free!");
    }
}

bool KineticAllocator_ValidateAllMemoryFreed(void)
{
    bool empty = (PDUList.start == NULL);
    LOG_LOCATION;
    // LOGF("  PDUList: 0x%0llX, empty=%s",
         // (long long)PDUList.start, empty ? "true" : "false");
    return empty;
}
