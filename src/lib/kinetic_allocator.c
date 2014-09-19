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

STATIC KineticPDUListItem* PDUList = NULL;
STATIC KineticPDUListItem* PDUListLast = NULL;

KineticPDU* KineticAllocator_NewPDU(void)
{
    KineticPDUListItem* newItem = (KineticPDUListItem*)malloc(sizeof(KineticPDUListItem));
    newItem->next = NULL;
    newItem->previous = NULL;

    if (PDUList == NULL)
    {
        PDUList = newItem;
    }
    else
    {
        newItem->previous = PDUListLast;
        PDUListLast->next = newItem;
    }
    PDUListLast = newItem;

    LOGF("Allocated new PDU list item @ 0x%0llX w/PDU @ 0x%0llX",
        (long long)newItem, (long long)&newItem->pdu);

    return &newItem->pdu;
}

void KineticAllocator_FreePDU(KineticPDU** pdu)
{
    KineticPDUListItem* cur = PDUList;
    while (&cur->pdu != *pdu)
    {
        if (cur->next == NULL)
        {
            LOG("  Reached end of list before finding PDU to free!");
            return;
        }
        else
        {
            LOG("  next..");
            cur = cur->next;
        }
    }
    LOG("  Done searching for PDU list item");

    if ((cur != NULL) && (&cur->pdu == *pdu))
    {
        LOG("  PDU found! freeing it.");
        // Handle PDU list emptied
        if (cur->previous == NULL)
        {
            LOG("  At start of list.");
            if (cur->next == NULL)
            {
                LOG("  Making it empty, since all deallocated!");
                PDUList = NULL;
                PDUListLast = NULL;
            }
            else
            {
                LOG("  Moving current item to head, since head deallocated!");
                PDUList = cur->next;
                PDUList->previous = NULL;
            }
        }
        else
        {
            // Relink from previous to next, if avaliable
            LOG("  Not at list start, so relinking list to free PDU.");
            if (cur->previous->next != NULL)
            {
                LOG("  Relinking previous to next");
                if (cur->next != NULL)
                {
                    LOG("    next being reset!");
                    cur->previous->next = cur->next;
                }
                else
                {
                    PDUListLast = cur->previous;
                    PDUListLast->next = NULL;
                    LOGF("    next is NULL. End of list now @ 0x%0llX",
                        (long long)PDUListLast);
                }
            }
            else
            {
                LOG("  This shouldn't happen!");
                PDUListLast = cur->previous;
            }
        }

        if ((cur->pdu.proto != NULL) && cur->pdu.protobufDynamicallyExtracted)
        {
            KineticProto__free_unpacked(cur->pdu.proto, NULL);
            cur->pdu.proto = NULL;
            cur->pdu.protobufDynamicallyExtracted = false;
        };

        LOGF("  Freeing item @ 0x%0llX, pdu @ 0x%0llX",
            (long long)cur, (long long)&cur->pdu);
        free(cur);
        cur = NULL;
    }

    *pdu = NULL;
}

void KineticAllocator_FreeAllPDUs(void)
{
    LOG_LOCATION;
    if (PDUList != NULL)
    {
        LOG("Freeing list of PDUs...");
        KineticPDUListItem* current = PDUList;

        while (current->next != NULL)
        {
            LOG("Advancing to next list item...");
            current = current->next;
        }

        while (current != NULL)
        {
            LOG("  Current item not freed!");
            LOGF("  DEALLOCATING item: 0x%0llX, pdu: 0x%llX, prev: 0x%0llX",
                (long long)current, (long long)&current->pdu, (long long)current->previous);
            KineticPDUListItem* curItem = current;
            KineticPDUListItem* prevItem = current->previous;
            if (curItem != NULL)
            {
                LOG("  Freeing list item");
                free(curItem); 
            }
            current = prevItem;
            LOGF("  on to prev=0x%llX", (long long)current);
        }
    }
    else
    {
        LOG("  Nothing to free!");
    }
    PDUList = NULL;
    PDUListLast = NULL;
}

bool KineticAllocator_ValidateAllMemoryFreed(void)
{
    bool empty = (PDUList == NULL);
    LOG_LOCATION; LOGF("  PDUList: 0x%0llX, empty=%s",
        (long long)PDUList, empty ? "true" : "false");
    return empty;
}
