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
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"
#include "unity.h"
#include "unity_helper.h"
#include <stdlib.h>
#include <pthread.h>

KineticConnection Connection;
KineticSession Session;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    KINETIC_CONNECTION_INIT(&Connection);

    KineticAllocator_InitLists(&Connection);

    pthread_mutex_t expectedMutex = PTHREAD_MUTEX_INITIALIZER;

    TEST_ASSERT_NULL(Connection.operations.start);
    TEST_ASSERT_NULL(Connection.operations.last);
    TEST_ASSERT_EQUAL_MEMORY(&expectedMutex, &Connection.operations.mutex, sizeof(pthread_mutex_t));
    TEST_ASSERT_FALSE(Connection.operations.locked);

    TEST_ASSERT_NULL(Connection.pdus.start);
    TEST_ASSERT_NULL(Connection.pdus.last);
    TEST_ASSERT_EQUAL_MEMORY(&expectedMutex, &Connection.pdus.mutex, sizeof(pthread_mutex_t));
    TEST_ASSERT_FALSE(Connection.pdus.locked);
}

void tearDown(void)
{
    bool allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);
    KineticAllocator_FreeAllPDUs(&Connection);
    KineticAllocator_FreeAllOperations(&Connection);

    TEST_ASSERT_NULL(Connection.operations.start);
    TEST_ASSERT_NULL(Connection.operations.last);
    TEST_ASSERT_FALSE(Connection.operations.locked);

    TEST_ASSERT_NULL(Connection.pdus.start);
    TEST_ASSERT_NULL(Connection.pdus.last);
    TEST_ASSERT_FALSE(Connection.pdus.locked);

    TEST_ASSERT_TRUE_MESSAGE(allFreed, "Dynamically allocated things were not freed!");

    KineticLogger_Close();
}

void test_KineticAllocator_ValidateAllMemoryFreed_should_return_true_if_all_allocated_things_have_been_freed(void)
{
    LOG_LOCATION;
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}


//==============================================================================
// PDU List Support
//==============================================================================

void test_KineticAllocator_GetFirstPDU_should_return_the_first_PDU_in_the_list(void)
{
    LOG_LOCATION;
    KineticPDU* pdus[] = {NULL, NULL, NULL};

    TEST_ASSERT_NULL(KineticAllocator_GetFirstPDU(&Connection));

    pdus[0] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_EQUAL_PTR(pdus[0], KineticAllocator_GetFirstPDU(&Connection));

    pdus[1] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_EQUAL_PTR(pdus[0], KineticAllocator_GetFirstPDU(&Connection));

    KineticAllocator_FreeAllPDUs(&Connection);
    TEST_ASSERT_NULL(KineticAllocator_GetFirstPDU(&Connection));
}


void test_KineticAllocator_GetNextPDU_should_return_the_next_PDU_in_the_list(void)
{
    LOG_LOCATION;
    KineticPDU* pdus[] = {NULL, NULL, NULL};

    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, NULL));

    pdus[0] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, NULL));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[0]));

    pdus[1] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_EQUAL_PTR(pdus[1], KineticAllocator_GetNextPDU(&Connection, pdus[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[1]));

    pdus[2] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_EQUAL_PTR(pdus[2], KineticAllocator_GetNextPDU(&Connection, pdus[1]));
    TEST_ASSERT_EQUAL_PTR(pdus[1], KineticAllocator_GetNextPDU(&Connection, pdus[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[2]));

    KineticAllocator_FreePDU(&Connection, pdus[2]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[1]));
    TEST_ASSERT_EQUAL_PTR(pdus[1], KineticAllocator_GetNextPDU(&Connection, pdus[0]));
    KineticAllocator_FreePDU(&Connection, pdus[0]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[1]));

    // Should return NULL if passed the address of a non-existent/freed PDU
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[2]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, NULL));

    KineticAllocator_FreePDU(&Connection, pdus[1]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[1]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextPDU(&Connection, pdus[2]));
}

void test_KineticAllocator_FreeAllPDUs_should_free_full_list_of_PDUs(void)
{
    LOG_LOCATION;
    KINETIC_CONNECTION_INIT(&Connection);

    KineticAllocator_NewPDU(&Connection);
    KineticAllocator_NewPDU(&Connection);
    KineticAllocator_NewPDU(&Connection);

    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));

    KineticAllocator_FreeAllPDUs(&Connection);

    bool allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);

    TEST_ASSERT_TRUE(allFreed);
}

void test_KineticAllocator_NewPDU_should_allocate_new_PDUs_and_store_references(void)
{
    LOG_LOCATION;
    KineticPDU* pdu;

    pdu = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdu);
    TEST_ASSERT_EQUAL_PTR(&Connection, pdu->connection);

    TEST_ASSERT_NOT_NULL(Connection.pdus.start);
    
    TEST_ASSERT_NULL(Connection.pdus.start->previous);
    TEST_ASSERT_NULL(Connection.pdus.start->next);
    TEST_ASSERT_NULL(Connection.pdus.last->next);
    TEST_ASSERT_NULL(Connection.pdus.last->previous);

    pdu = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdu);
    TEST_ASSERT_EQUAL_PTR(&Connection, pdu->connection);
    TEST_ASSERT_NOT_NULL(Connection.pdus.start->next);

    pdu = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdu);
    TEST_ASSERT_EQUAL_PTR(&Connection, pdu->connection);
    TEST_ASSERT_NOT_NULL(Connection.pdus.start->next);

    KineticAllocator_FreeAllPDUs(&Connection);
}


void test_KineticAllocator_should_allocate_and_free_a_single_PDU_list_item(void)
{
    LOG_LOCATION;
    KineticPDU* pdu;
    bool allFreed = false;

    pdu = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdu);

    KineticAllocator_FreePDU(&Connection, pdu);

    allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);
    KineticAllocator_FreeAllPDUs(&Connection); // Just so we don't leak memory upon failure...
    TEST_ASSERT_TRUE(allFreed);
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_PDU_list_items_in_stacked_order(void)
{
    LOG_LOCATION;
    KineticPDU* pdus[2];

    pdus[0] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[0]);
    pdus[1] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[1]);

    KineticAllocator_FreePDU(&Connection, pdus[1]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreePDU(&Connection, pdus[0]);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_PDU_list_items_in_allocation_order(void)
{
    LOG_LOCATION;
    KineticPDU* pdus[2];

    pdus[0] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[0]);
    pdus[1] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[1]);

    KineticAllocator_FreePDU(&Connection, pdus[0]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreePDU(&Connection, pdus[1]);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}

void test_KineticAllocator_should_allocate_and_free_multiple_PDU_list_items_in_random_order(void)
{
    LOG_LOCATION;
    KineticPDU* pdus[3];

    pdus[0] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[0]);
    pdus[1] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[1]);
    pdus[2] = KineticAllocator_NewPDU(&Connection);
    TEST_ASSERT_NOT_NULL(pdus[2]);

    KineticAllocator_FreePDU(&Connection, pdus[1]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreePDU(&Connection, pdus[0]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreePDU(&Connection, pdus[2]);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}

void test_KineticAllocator_GetFirstOperation_should_return_the_first_Operation_in_the_list(void)
{
    LOG_LOCATION;
    KineticOperation* operations[] = {NULL, NULL, NULL};

    TEST_ASSERT_NULL(KineticAllocator_GetFirstOperation(&Connection));
    operations[0] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_EQUAL_PTR(operations[0], KineticAllocator_GetFirstOperation(&Connection));
    // operations[1] = KineticAllocator_NewOperation(&Connection);
    // TEST_ASSERT_EQUAL_PTR(operations[0], KineticAllocator_GetFirstOperation(&Connection));

    KineticAllocator_FreeAllOperations(&Connection);
    TEST_ASSERT_NULL(KineticAllocator_GetFirstOperation(&Connection));
}

void test_KineticAllocator_GetNextOperation_should_return_the_next_Operation_in_the_list(void)
{
    LOG_LOCATION;
    KineticOperation* operations[] = {NULL, NULL, NULL};

    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, NULL));

    operations[0] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, NULL));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[0]));

    operations[1] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_EQUAL_PTR(operations[1], KineticAllocator_GetNextOperation(&Connection, operations[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[1]));

    operations[2] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_EQUAL_PTR(operations[2], KineticAllocator_GetNextOperation(&Connection, operations[1]));
    TEST_ASSERT_EQUAL_PTR(operations[1], KineticAllocator_GetNextOperation(&Connection, operations[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[2]));

    KineticAllocator_FreeOperation(&Connection, operations[2]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[1]));
    TEST_ASSERT_EQUAL_PTR(operations[1], KineticAllocator_GetNextOperation(&Connection, operations[0]));
    KineticAllocator_FreeOperation(&Connection, operations[0]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[1]));

    // Should return NULL if passed the address of a non-existent/freed Operation
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[2]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, NULL));

    KineticAllocator_FreeOperation(&Connection, operations[1]);
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[0]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[1]));
    TEST_ASSERT_NULL(KineticAllocator_GetNextOperation(&Connection, operations[2]));
}

void test_KineticAllocator_FreeAllOperations_should_free_full_list_of_Operations(void)
{
    LOG_LOCATION;
    KINETIC_CONNECTION_INIT(&Connection);
    KineticOperation* operations[3];

    operations[0] = KineticAllocator_NewOperation(&Connection);
    operations[1] = KineticAllocator_NewOperation(&Connection);
    operations[2] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));

    KineticAllocator_FreePDU(&Connection, operations[0]->request);
    operations[1]->response = KineticAllocator_NewPDU(&Connection);

    KineticAllocator_FreeAllOperations(&Connection);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}

void test_KineticAllocator_NewOperation_should_allocate_new_Operations_and_store_references(void)
{
    LOG_LOCATION;
    KineticOperation* operation;

    operation = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operation);
    TEST_ASSERT_EQUAL_PTR(&Connection, operation->connection);
    TEST_ASSERT_NOT_NULL(Connection.operations.start);
    TEST_ASSERT_NULL(Connection.operations.start->previous);
    TEST_ASSERT_NULL(Connection.operations.start->next);
    TEST_ASSERT_NULL(Connection.operations.last->next);
    TEST_ASSERT_NULL(Connection.operations.last->previous);

    operation = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operation);
    TEST_ASSERT_NOT_NULL(Connection.operations.start->next);

    operation = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operation);
    TEST_ASSERT_NOT_NULL(Connection.operations.start->next);

    KineticAllocator_FreeAllOperations(&Connection);
}


void test_KineticAllocator_should_allocate_and_free_a_single_Operation_list_item_with_request_PDU(void)
{
    LOG_LOCATION;
    KineticOperation* operation;

    operation = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operation);
    TEST_ASSERT_EQUAL_PTR(&Connection, operation->connection);
    TEST_ASSERT_NOT_NULL(operation->request);
    TEST_ASSERT_NOT_NULL(operation->request->proto);
    TEST_ASSERT_NOT_NULL(operation->request->command);
    TEST_ASSERT_EQUAL(KINETIC_PDU_TYPE_REQUEST, operation->request->type);
    TEST_ASSERT_NULL(operation->response);

    KineticAllocator_FreeOperation(&Connection, operation);

    bool allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);
    KineticAllocator_FreeAllOperations(&Connection); // Just so we don't leak memory upon failure...
    TEST_ASSERT_TRUE(allFreed);
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_Operation_list_items_in_stacked_order(void)
{
    LOG_LOCATION;
    KineticOperation* operations[2];

    operations[0] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[0]);
    operations[1] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[1]);

    KineticAllocator_FreeOperation(&Connection, operations[1]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreeOperation(&Connection, operations[0]);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}

void test_KineticAllocator_should_allocate_and_free_a_pair_of_Operation_list_items_in_allocation_order(void)
{
    LOG_LOCATION;
    KineticOperation* operations[2];
    bool allFreed = false;

    operations[0] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[0]);
    operations[1] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[1]);

    KineticAllocator_FreeOperation(&Connection, operations[0]);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);
    if (allFreed) {
        LOG0("Failed validating Operation freed!");
    }
    TEST_ASSERT_FALSE(allFreed);

    KineticAllocator_FreeOperation(&Connection, operations[1]);
    allFreed = KineticAllocator_ValidateAllMemoryFreed(&Connection);
    if (!allFreed) {
        LOG0("Failed validating Operation freed!");
    }
    TEST_ASSERT_TRUE(allFreed);
}

void test_KineticAllocator_should_allocate_and_free_multiple_Operation_list_items_in_random_order(void)
{
    LOG_LOCATION;
    KineticOperation* operations[3];

    operations[0] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[0]);
    operations[1] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[1]);
    operations[2] = KineticAllocator_NewOperation(&Connection);
    TEST_ASSERT_NOT_NULL(operations[2]);

    KineticAllocator_FreeOperation(&Connection, operations[1]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreeOperation(&Connection, operations[0]);
    TEST_ASSERT_FALSE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
    KineticAllocator_FreeOperation(&Connection, operations[2]);
    TEST_ASSERT_TRUE(KineticAllocator_ValidateAllMemoryFreed(&Connection));
}
