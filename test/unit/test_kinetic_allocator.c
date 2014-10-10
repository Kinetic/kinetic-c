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
#include "kinetic_types_internal.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "unity.h"
#include "unity_helper.h"
#include <stdlib.h>

extern bool listsLocked;

KineticSession Session;
KineticList PDUList;

void setUp(void)
{
    KineticLogger_Init(NULL);
    TEST_ASSERT_NULL(PDUList.start);
    TEST_ASSERT_NULL(PDUList.last);
    listsLocked = false;
    TEST_ASSERT_FALSE(listsLocked);
}

void tearDown(void)
{
    LOG_LOCATION;
    bool allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    KineticAllocator_FreeAllPDUs(&PDUList);
    TEST_ASSERT_NULL(PDUList.start);
    TEST_ASSERT_NULL(PDUList.last);
    TEST_ASSERT_TRUE_MESSAGE(allFreed, "Dynamically allocated things were not freed!");
    TEST_ASSERT_FALSE(listsLocked);
}


void test_KineticAllocator_FreeAllPDUs_should_free_full_list_of_PDUs(void)
{
    LOG_LOCATION;
    const int count = 3;
    KineticListItem* list[count];

    // Allocate some PDUs and list items to hold them
    for (int i = 0; i < count; i++) {
        list[i] = (KineticListItem*)malloc(sizeof(KineticListItem));
        LOGF("ALLOCATED item[%d]: 0x%0llX", i, (long long)list[i]);
    }

    // Allocate the double-linked list
    list[0]->previous = NULL;       list[0]->next = list[1];
    list[1]->previous = list[0];    list[1]->next = list[2];
    list[2]->previous = list[1];    list[2]->next = NULL;
    PDUList.start = list[0];
    PDUList.last  = list[2];

    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&PDUList));

    KineticAllocator_FreeAllPDUs(&PDUList);

    bool allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);

    TEST_ASSERT_TRUE(allFreed);
}


void test_KineticAllocator_ValidateAllMemoryFreed_should_return_true_if_all_PDUs_have_been_freed(void)
{
    LOG_LOCATION;
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&PDUList));
}

void test_KineticAllocator_NewPDU_should_allocate_new_PDUs_and_store_references(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KineticPDU* pdu;

    pdu = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu);
    pdu->connection = &connection;
    TEST_ASSERT_NOT_NULL(PDUList.start);
    TEST_ASSERT_NULL(PDUList.start->previous);
    TEST_ASSERT_NULL(PDUList.start->next);
    TEST_ASSERT_NULL(PDUList.last->next);
    TEST_ASSERT_NULL(PDUList.last->previous);

    pdu = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu);
    pdu->connection = &connection;
    TEST_ASSERT_NOT_NULL(PDUList.start->next);

    pdu = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu);
    pdu->connection = &connection;
    TEST_ASSERT_NOT_NULL(PDUList.start->next);

    KineticAllocator_FreeAllPDUs(&PDUList);
}


void test_KineticAllocator_should_allocate_and_free_a_single_PDU_list_item(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KineticPDU* pdu0;
    bool allFreed = false;

    pdu0 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu0);
    pdu0->connection = &connection;

    KineticAllocator_FreePDU(&PDUList, pdu0);

    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    TEST_ASSERT_TRUE(allFreed);
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_PDU_list_items_in_stacked_order(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KineticPDU* pdu0, *pdu1;
    bool allFreed = false;

    LOG("Allocating first PDU");
    pdu0 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu0);
    pdu0->connection = &connection;

    LOG("Allocating second PDU");
    pdu1 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu1);
    pdu1->connection = &connection;

    LOG("Freeing second PDU");
    KineticAllocator_FreePDU(&PDUList, pdu1);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_FALSE(allFreed);

    LOG("Freeing first PDU");
    KineticAllocator_FreePDU(&PDUList, pdu0);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (!allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_TRUE(allFreed);

    LOG("PASSED!");
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_PDU_list_items_in_allocation_order(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KineticPDU* pdu0, *pdu1;
    bool allFreed = false;

    LOG("Allocating first PDU");
    pdu0 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu0);
    pdu0->connection = &connection;

    LOG("Allocating second PDU");
    pdu1 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu1);
    pdu1->connection = &connection;

    LOG("Freeing first PDU");
    KineticAllocator_FreePDU(&PDUList, pdu0);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_FALSE(allFreed);

    LOG("Freeing second PDU");
    KineticAllocator_FreePDU(&PDUList, pdu1);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (!allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_TRUE(allFreed);

    LOG("PASSED!");
}

void test_KineticAllocator_should_allocate_and_free_multiple_PDU_list_items_in_random_order(void)
{
    LOG_LOCATION;
    KineticConnection connection;
    KineticPDU* pdu0, *pdu1, *pdu2;
    bool allFreed = false;

    LOG("Allocating first PDU");
    pdu0 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu0);
    pdu0->connection = &connection;

    LOG("Allocating second PDU");
    pdu1 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu1);
    pdu1->connection = &connection;

    LOG("Allocating third PDU");
    pdu2 = KineticAllocator_NewPDU(&PDUList, &connection);
    TEST_ASSERT_NOT_NULL(pdu2);
    pdu2->connection = &connection;

    LOG("Freeing second PDU");
    KineticAllocator_FreePDU(&PDUList, pdu1);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_FALSE(allFreed);

    LOG("Freeing first PDU");
    KineticAllocator_FreePDU(&PDUList, pdu0);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_FALSE(allFreed);

    LOG("Freeing third PDU");
    KineticAllocator_FreePDU(&PDUList, pdu2);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&PDUList);
    if (!allFreed) {
        LOG("Failed validating PDU freed!");
        KineticAllocator_FreeAllPDUs(&PDUList); // Just so we don't leak memory upon failure...
    }
    TEST_ASSERT_TRUE(allFreed);

    LOG("PASSED!");
}
