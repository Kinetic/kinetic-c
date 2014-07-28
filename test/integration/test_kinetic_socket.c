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
#include "kinetic_logger.h"
#include <protobuf-c/protobuf-c.h>
#include "kinetic_proto.h"
#include "kinetic_message.h"

static int FileDesc;
static int KineticTestPort = KINETIC_PORT; //8999;
const char* TestData = "Some like it hot!";

void setUp(void)
{
    FileDesc = -1;
    KineticLogger_Init(NULL); // Log to stderr
}

void tearDown(void)
{
    if (FileDesc > 0)
    {
        KineticSocket_Close(FileDesc);
    }
}

void test_KineticSocket_KINETIC_PORT_should_be_8123(void)
{
    TEST_ASSERT_EQUAL(8123, KINETIC_PORT);
}

void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}

void test_KineticSocket_Write_should_write_the_data_to_the_specified_socket(void)
{
    bool success = false;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    success = KineticSocket_Write(FileDesc, TestData, strlen(TestData));

    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");
}

void test_KineticSocket_WriteProtobuf_should_write_serialized_protobuf_to_the_specified_socket(void)
{
    bool success = false;
    KineticMessage msg;

    KineticMessage_Init(&msg);
    msg.header.clusterversion = 12345678;
    msg.header.has_clusterversion = true;
    msg.header.identity = -12345678;
    msg.header.has_identity = true;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    success = KineticSocket_WriteProtobuf(FileDesc, &msg);

    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");
}

void test_KineticSocket_Read_should_read_data_from_the_specified_socket(void)
{
    bool success = false;
    const char* readRequest = "read(5)\0";
    char data[5];

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    // Send request to test server to send us some data
    success = KineticSocket_Write(FileDesc, readRequest, strlen(readRequest));
    TEST_ASSERT_TRUE(success);

    success = KineticSocket_Read(FileDesc, data, sizeof(data));

    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to read from socket!");
}

void test_KineticSocket_Read_should_timeout_if_requested_data_is_not_received_within_configured_timeout(void)
{
    bool success = false;
    char data[64];

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    success = KineticSocket_Read(FileDesc, data, sizeof(data));

    TEST_ASSERT_FALSE_MESSAGE(success, "Expected socket to timeout waiting on data!");
}

void test_KineticSocket_ReadProtobuf_should_read_the_specified_length_of_an_encoded_protobuf_from_the_specified_socket(void)
{
    bool success = false;
    const char* readRequest = "readProto()\0";
    uint8_t data[256];
    uint8_t protoBuf[1024*1024];
    KineticProto* proto = (KineticProto*)protoBuf;
    size_t expectedLength = 125; // This would normally be extracted from the PDU header

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    // Send request to test server to send us a Kinetic protobuf
    success = KineticSocket_Write(FileDesc, readRequest, strlen(readRequest));
    TEST_ASSERT_TRUE(success);

    success = KineticSocket_ReadProtobuf(FileDesc, proto, data, expectedLength);

    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to read from socket!");

    printf("Received Kinetic protobuf:\n  identity: %016llX\n",
        (unsigned long long)proto->command->header->identity);
}

void test_KineticSocket_ReadProtobuf_should_return_false_if_KineticProto_of_specified_length_fails_to_be_read_within_timeout(void)
{
    bool success = false;
    const char* readRequest = "readProto()\0";
    uint8_t data[256];
    uint8_t protoBuf[1024*1024];
    KineticProto* proto = (KineticProto*)protoBuf;
    size_t expectedLength = 150; // This would normally be extracted from the PDU header

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);

    // Send request to test server to send us a Kinetic protobuf
    success = KineticSocket_Write(FileDesc, readRequest, strlen(readRequest));
    TEST_ASSERT_TRUE(success);

    success = KineticSocket_ReadProtobuf(FileDesc, proto, data, expectedLength);

    TEST_ASSERT_FALSE_MESSAGE(success, "Expected timeout!");
}
