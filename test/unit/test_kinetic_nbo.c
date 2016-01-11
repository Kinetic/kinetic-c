/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
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
