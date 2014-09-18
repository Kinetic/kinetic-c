/*
* kinetic-c-client
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_types.h"
#include "unity.h"
#include "unity_helper.h"

extern const int KineticStatusDescriptorCount;

void setUp(void)
{
}

void tearDown(void)
{
}

void test_kinetic_types_should_be_defined(void)
{
    ByteArray array; (void)array;
    ByteBuffer buffer; (void)buffer;
    KineticAlgorithm algorithm; (void)algorithm;
    KineticSynchronization synchronization; (void)synchronization;
    KineticSessionHandle sessionHandle; (void)sessionHandle;
    KineticSession sessionConfig; (void)sessionConfig;
    KineticOperationHandle operationHandle; (void)operationHandle;
    KineticStatus status; (void)status;
}

void test_KineticStatus_should_have_mapped_descriptors(void)
{
    TEST_ASSERT_EQUAL(KINETIC_STATUS_COUNT, KineticStatusDescriptorCount);
}
