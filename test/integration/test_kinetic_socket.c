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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif // _BSD_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "byte_array.h"
#include "socket99/socket99.h"
#include "protobuf-c/protobuf-c.h"

#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_socket.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"


static int FileDesc;
static int KineticTestPort = KINETIC_PORT;
static ByteArray TestData;
static bool LogInitialized = false;
static KineticPDU PDU;

void setUp(void)
{
    FileDesc = -1;
    if (!LogInitialized) {
        KineticLogger_Init(NULL);//"test_kinetic_socket.log");
        LogInitialized = true;
    }
    TestData = ByteArray_CreateWithCString("Some like it hot!");
}

void tearDown(void)
{
    if (FileDesc >= 0) {
        LOG("Shutting down socket...");
        KineticSocket_Close(FileDesc);
        FileDesc = 0;
    }
}


void test_KineticSocket_KINETIC_PORT_should_be_8123(void)
{
    LOG_LOCATION;
    TEST_ASSERT_EQUAL(8123, KINETIC_PORT);
}


void test_KineticSocket_Connect_should_create_a_socket_connection(void)
{
    LOG_LOCATION;
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
}


// Disabling socket read/write tests in not OSX, since Linux TravisCI builds
// fail, but system test passes. Most likely an issue with KineticRuby server
#if defined(__APPLE__)

void test_KineticSocket_Write_should_write_the_data_to_the_specified_socket(void)
{
    LOG_LOCATION;
    bool success = false;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    ByteBuffer sendBuffer = ByteBuffer_CreateWithArray(TestData);
    success = KineticSocket_Write(FileDesc, &sendBuffer);
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");
    TEST_ASSERT_EQUAL(TestData.len, sendBuffer.bytesUsed);

    LOG("Flushing socket read pipe...");
    uint8_t bufferData[40];
    ByteBuffer recvBuffer = ByteBuffer_Create(bufferData, sizeof(bufferData));
    KineticSocket_Read(FileDesc, &recvBuffer);
}

void test_KineticSocket_WriteProtobuf_should_write_serialized_protobuf_to_the_specified_socket(void)
{
    LOG_LOCATION;
    bool success = false;
    KineticSession session = {
        .clusterVersion = 12345678,
        .identity = -12345678,
    };
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    connection.session = session;
    KINETIC_PDU_INIT_WITH_MESSAGE(&PDU, &connection);
    KineticMessage_Init(&PDU.protoData.message);
    PDU.header.protobufLength = KineticProto__get_packed_size(PDU.proto);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    LOG("Writing a dummy protobuf...");
    success = KineticSocket_WriteProtobuf(FileDesc, &PDU);
    TEST_ASSERT_TRUE_MESSAGE(success, "Failed to write to socket!");

    LOG("Flushing socket read pipe...");
    uint8_t bufferData[40];
    ByteBuffer recvBuffer = ByteBuffer_Create(bufferData, sizeof(bufferData));
    KineticSocket_Read(FileDesc, &recvBuffer);
}


void test_KineticSocket_Read_should_read_data_from_the_specified_socket(void)
{
    LOG_LOCATION;
    bool success = false;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us some data
    ByteArray requestArray = ByteArray_CreateWithCString("read(5)");
    ByteBuffer requestBuffer = ByteBuffer_CreateWithArray(requestArray);
    success = KineticSocket_Write(FileDesc, &requestBuffer);
    TEST_ASSERT_TRUE(success);

    uint8_t responseData[5];
    ByteBuffer responseBuffer =
        ByteBuffer_Create(responseData, sizeof(responseData));
    TEST_ASSERT_EQUAL(0, responseBuffer.bytesUsed);
    success = KineticSocket_Read(FileDesc, &responseBuffer);

    TEST_ASSERT_TRUE_MESSAGE(success,
                             "Failed to read from socket!");
    TEST_ASSERT_EQUAL_MESSAGE(5, responseBuffer.bytesUsed,
                              "Received incorrect number of bytes");
}

void test_KineticSocket_Read_should_timeout_if_requested_data_is_not_received_within_configured_timeout(void)
{
    LOG_LOCATION;
    bool success = false;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us some data
    ByteArray requestArray = ByteArray_CreateWithCString("read(5)");
    ByteBuffer requestBuffer = ByteBuffer_CreateWithArray(requestArray);
    success = KineticSocket_Write(FileDesc, &requestBuffer);
    TEST_ASSERT_TRUE(success);

    // Try to read more than was requested, to cause a timeout
    uint8_t responseData[7];
    ByteBuffer responseBuffer =
        ByteBuffer_Create(responseData, sizeof(responseData));
    TEST_ASSERT_EQUAL(0, responseBuffer.bytesUsed);
    success = KineticSocket_Read(FileDesc, &responseBuffer);

    TEST_ASSERT_FALSE_MESSAGE(success,
                              "Expected socket to timeout waiting on data!");
}


void test_KineticSocket_ReadProtobuf_should_read_the_specified_length_of_an_encoded_protobuf_from_the_specified_socket(void)
{
    LOG_LOCATION;
    bool success = false;
    KineticSession session = {
        .clusterVersion = 12345678,
        .identity = -12345678,
    };
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    connection.session = session;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us a Kinetic protobuf
    ByteArray requestArray = ByteArray_CreateWithCString("readProto()");
    ByteBuffer requestBuffer = ByteBuffer_CreateWithArray(requestArray);
    success = KineticSocket_Write(FileDesc, &requestBuffer);
    TEST_ASSERT_TRUE(success);

    // Receive the response
    KINETIC_PDU_INIT(&PDU, &connection);
    PDU.header.protobufLength = 125;
    TEST_ASSERT_FALSE(PDU.protobufDynamicallyExtracted);
    TEST_ASSERT_NULL(PDU.proto);
    success = KineticSocket_ReadProtobuf(FileDesc, &PDU);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_NOT_NULL_MESSAGE(PDU.proto,
                                 "Protobuf pointer was NULL, but expected dynamic memory allocation!");
    TEST_ASSERT_TRUE_MESSAGE(PDU.protobufDynamicallyExtracted,
                             "Flag was not set per dynamically allocated/extracted protobuf");
    LOG("Received Kinetic protobuf:");
    LOGF("  command: (0x%zX)", (size_t)PDU.proto->command);
    LOGF("    header: (0x%zX)", (size_t)PDU.proto->command->header);
    LOGF("      identity: %016llX",
         (unsigned long long)PDU.proto->command->header->identity);
    KineticProto__free_unpacked(PDU.proto, NULL);
    ByteArray hmacArray = {
        .data = PDU.proto->hmac.data, .len = PDU.proto->hmac.len
    };
    KineticLogger_LogByteArray("  hmac", hmacArray);

    LOG("Kinetic ProtoBuf read successfully!");
}

void test_KineticSocket_ReadProtobuf_should_return_false_if_KineticProto_of_specified_length_fails_to_be_read_within_timeout(void)
{
    LOG_LOCATION;
    bool success = false;
    KineticSession session = {
        .clusterVersion = 12345678,
        .identity = -12345678,
    };
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    connection.session = session;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    // Send request to test server to send us a Kinetic protobuf
    ByteArray requestArray = ByteArray_CreateWithCString("readProto()");
    ByteBuffer requestBuffer = ByteBuffer_CreateWithArray(requestArray);
    success = KineticSocket_Write(FileDesc, &requestBuffer);
    TEST_ASSERT_TRUE(success);

    // Receive the dummy protobuf response, but expect too much data
    // to force timeout
    KINETIC_PDU_INIT(&PDU, &connection);
    PDU.header.protobufLength = 1000;
    TEST_ASSERT_FALSE(PDU.protobufDynamicallyExtracted);
    TEST_ASSERT_NULL(PDU.proto);
    success = KineticSocket_ReadProtobuf(FileDesc, &PDU);
    TEST_ASSERT_FALSE_MESSAGE(success, "Expected timeout!");
    TEST_ASSERT_FALSE_MESSAGE(PDU.protobufDynamicallyExtracted,
                              "Protobuf should not have been extracted because of timeout");
    TEST_ASSERT_NULL_MESSAGE(PDU.proto,
                             "Protobuf should not have been allocated because of timeout");
}

#endif // defined(__APPLE__)
