/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_nbo.h"
#include <arpa/inet.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticNBO_FromHostU32_should_convert_uint32_from_host_to_network_byte_order(void)
{
    uint32_t host = 0x12345678u;
    uint32_t net = KineticNBO_FromHostU32(host);
    TEST_ASSERT_EQUAL_HEX32(0x78563412u, net);
}

void test_KineticNBO_ToHostU32_should_convert_uint32_from_network_to_host_byte_order(void)
{
    uint32_t net = 0x78563412u;
    uint32_t host = KineticNBO_ToHostU32(net);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, host);
}

void test_KineticNBO_FromHostU64_should_convert_uint64_from_host_to_network_byte_order(void)
{
    uint64_t host = 0x1234567890ABCDEFu;
    uint64_t net = KineticNBO_FromHostU64(host);
    TEST_ASSERT_EQUAL_HEX64(0xEFCDAB9078563412u, net);
}

void test_KineticNBO_ToHostU64_should_convert_uint64_from_network_to_host_byte_order(void)
{
    uint64_t net = 0xEFCDAB9078563412u;
    uint64_t host = KineticNBO_ToHostU64(net);
    TEST_ASSERT_EQUAL_HEX64(0x1234567890ABCDEFu, host);
}
