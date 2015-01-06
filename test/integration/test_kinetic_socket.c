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

#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"

#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "socket99.h"

static int FileDesc;
static int KineticTestPort = KINETIC_PORT;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    FileDesc = -1;
}

void tearDown(void)
{
    if (FileDesc >= 0) {
        LOG0("Shutting down socket...");
        KineticSocket_Close(FileDesc);
        FileDesc = 0;
    }
    KineticLogger_Close();
}


void test_KineticSocket_KINETIC_PORT_should_be_8123(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(8123, KINETIC_PORT);
}


void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    LOG_LOCATION;
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}



