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

#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
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
// static int KineticTestPort = KINETIC_PORT;

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
    TEST_ASSERT_EQUAL(8123, KINETIC_PORT);
}


void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    TEST_IGNORE_MESSAGE("Reinstantiate this test once test host is rewired in");
    // FileDesc = KineticSocket_Connect(SYSTEM_TEST_HOST, KineticTestPort);
    // TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}
