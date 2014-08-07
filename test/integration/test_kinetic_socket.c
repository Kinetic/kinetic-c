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

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static int FileDesc;
static int KineticTestPort = KINETIC_PORT /*8999*/;
static const char* TestData = "Some like it hot!";
static KineticMessage Msg;
static KineticProto* pProto;
static bool LogInitialized = false;

void setUp(void)
{
    FileDesc = -1;
    pProto = NULL;
    if (!LogInitialized)
    {
        KineticLogger_Init(NULL);//"test_kinetic_socket.log");
        LogInitialized = true;
    }
    LOG("--------------------------------------------------------------------------------");
}

void tearDown(void)
{
    // Free the allocated protobuf so we don't leak memory!
    if (pProto)
    {
        LOG("Freeing protobuf...");
        KineticProto_free_unpacked(pProto, NULL);
    }

    if (FileDesc >= 0)
    {
        LOG("Shutting down socket...");
        KineticSocket_Close(FileDesc);
        FileDesc = 0;
        sleep(1);
    }
}

void test_KineticSocket_KINETIC_PORT_should_be_8123(void)
{
    LOG(__func__);
    TEST_ASSERT_EQUAL(8123, KINETIC_PORT);
}

void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    LOG(__func__);
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}

void test_KineticSocket_Write_should_write_the_data_to_the_specified_socket(void)
{
    bool success = false;
    char data[40];
    LOG(__func__);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    success = KineticSocket_Write(FileDesc, TestData, strlen(TestData));
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");

    LOG("Flushing socket read pipe...");
    KineticSocket_Read(FileDesc, data, sizeof(data));
}

void test_KineticSocket_WriteProtobuf_should_write_serialized_protobuf_to_the_specified_socket(void)
{
    bool success = false;
    LOG(__func__);
    char data[5];

    KineticMessage_Init(&Msg);
    Msg.header.clusterversion = 12345678;
    Msg.header.has_clusterversion = true;
    Msg.header.identity = -12345678;
    Msg.header.has_identity = true;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    LOG("Writing a dummy protobuf...");
    success = KineticSocket_WriteProtobuf(FileDesc, &Msg.proto);
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");

    LOG("Flushing socket read pipe...");
    KineticSocket_Read(FileDesc, data, sizeof(data));
}

void test_KineticSocket_Read_should_read_data_from_the_specified_socket(void)
{
    bool success = false;
    const char* readRequest = "read(5)";
    char data[5];
    LOG(__func__);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

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
    LOG(__func__);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    success = KineticSocket_Read(FileDesc, data, sizeof(data));

    TEST_ASSERT_FALSE_MESSAGE(success, "Expected socket to timeout waiting on data!");
}

void test_KineticSocket_ReadProtobuf_should_read_the_specified_length_of_an_encoded_protobuf_from_the_specified_socket(void)
{
    bool success = false;
    const char* readRequest = "readProto()";
    uint8_t data[1024*1024];
    size_t expectedLength = 125; // This would normally be extracted from the PDU header
    LOG(__func__);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us a Kinetic protobuf
    success = KineticSocket_Write(FileDesc, readRequest, strlen(readRequest));
    TEST_ASSERT_TRUE(success);

    // Receive the response
    success = KineticSocket_ReadProtobuf(FileDesc, &pProto, data, expectedLength);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_NOT_NULL_MESSAGE(pProto, "Protobuf pointer was NULL, but expected dynamic memory allocation!");
    LOG( "Received Kinetic protobuf:");
    LOGF("  command: (0x%zX)", (size_t)pProto->command);
    LOGF("    header: (0x%zX)", (size_t)pProto->command->header);
    LOGF("      identity: %016llX", (unsigned long long)pProto->command->header->identity);
    LOGF("  hmac: (%zd bytes): %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
        pProto->hmac.len,
        pProto->hmac.data[0], pProto->hmac.data[1], pProto->hmac.data[2], pProto->hmac.data[3],
        pProto->hmac.data[4], pProto->hmac.data[5], pProto->hmac.data[6], pProto->hmac.data[7],
        pProto->hmac.data[8], pProto->hmac.data[9], pProto->hmac.data[10], pProto->hmac.data[11],
        pProto->hmac.data[12], pProto->hmac.data[13], pProto->hmac.data[14], pProto->hmac.data[15],
        pProto->hmac.data[16], pProto->hmac.data[17], pProto->hmac.data[18], pProto->hmac.data[19]);

    LOG("Kinetic ProtoBuf read successfully!");
}

void test_KineticSocket_ReadProtobuf_should_return_false_if_KineticProto_of_specified_length_fails_to_be_read_within_timeout(void)
{
    bool success = false;
    const char* readRequest = "readProto()";
    uint8_t data[256];
    size_t expectedLength = 150; // This would normally be extracted from the PDU header
    LOG(__func__);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us a Kinetic protobuf
    success = KineticSocket_Write(FileDesc, readRequest, strlen(readRequest));
    TEST_ASSERT_TRUE(success);

    success = KineticSocket_ReadProtobuf(FileDesc, &pProto, data, expectedLength);
    TEST_ASSERT_FALSE_MESSAGE(success, "Expected timeout!");
    TEST_ASSERT_NULL_MESSAGE(pProto, "Protobuf pointer should not have gotten set, since no memory allocated.");
}
