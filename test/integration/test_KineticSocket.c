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

#include "KineticSocket.h"
#include "KineticLogger.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
    KineticLogger_Init(KINETIC_LOG_FILE);
}

void tearDown(void)
{
}

void test_KineticSocket_KINETIC_PORT_should_be_8213(void)
{
    TEST_ASSERT_EQUAL(8213, KINETIC_PORT);
}

void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    int fd = KineticSocket_Connect("localhost", 8999, true);

    TEST_ASSERT_MESSAGE(fd >= 0, "File descriptor invalid");

    KineticSocket_Close(fd);
}
