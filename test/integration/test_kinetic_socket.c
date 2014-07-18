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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "unity_helper.h"
#include "kinetic_socket.h"
#include "mock_kinetic_logger.h"
#include "unity.h"

static char Msg[1024];
static int FileDesc;
static int KineticTestPort = 8999;

void setUp(void)
{
    FileDesc = -1;
}

void tearDown(void)
{
    if (FileDesc > 0)
    {
        sprintf(Msg, "Closing socket with FileDesc=%d", FileDesc);
        KineticLogger_Log_Expect(Msg);
        KineticSocket_Close(FileDesc);
    }
}

void test_KineticSocket_KINETIC_PORT_should_be_8213(void)
{
    TEST_ASSERT_EQUAL(8213, KINETIC_PORT);
}

void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    char msg[128];
    sprintf(msg, "Connecting to localhost:%d", KineticTestPort);
    KineticLogger_Log_Expect(msg);
    KineticLogger_Log_Expect("Trying to connect to host");
    KineticLogger_Log_Expect("Connected to host!");

    int FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    TEST_ASSERT_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}
