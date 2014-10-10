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

#include "socket99/socket99.h"

static int FileDesc;
static int KineticTestPort = KINETIC_PORT;
static uint8_t TestData[128];
static ByteBuffer TestDataBuffer;
static bool LogInitialized = false;
static bool SocketReadRequested;

void Socket_RequestBytes(size_t count)
{
    char request[10];
    sprintf(request, "read(%zu)", count);
    uint8_t requestData[64];
    ByteBuffer requestBuffer = ByteBuffer_Create(requestData, sizeof(requestData), 0);
    ByteBuffer_AppendCString(&requestBuffer, request);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SUCCESS, KineticSocket_Write(FileDesc, &requestBuffer),
        "Failed requesting dummy data from test socket server");
}

void Socket_RequestProtobuf(void)
{
    uint8_t requestData[64];
    ByteBuffer requestBuffer = ByteBuffer_Create(requestData, sizeof(requestData), 0);
    ByteBuffer_AppendCString(&requestBuffer, "readProto()");
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SUCCESS, KineticSocket_Write(FileDesc, &requestBuffer),
        "Failed requesting dummy protobuf from test socket server");
}

void Socket_FlushReadPipe(void)
{
    LOG("Flushing socket socket read pipe...");
    uint8_t bufferData[40];
    ByteBuffer recvBuffer = ByteBuffer_Create(bufferData, sizeof(bufferData), 0);
    KineticSocket_Read(FileDesc, &recvBuffer, sizeof(bufferData));
}


void setUp(void)
{
    FileDesc = -1;
    SocketReadRequested = false;
    if (!LogInitialized) {
        KineticLogger_Init(NULL);//"test_kinetic_socket.log");
        LogInitialized = true;
    }
    TestDataBuffer = ByteBuffer_Create(TestData, sizeof(TestData), 0);
    ByteBuffer_AppendCString(&TestDataBuffer, "Some like it hot!");
}

void tearDown(void)
{
    if (FileDesc >= 0) {
        if (SocketReadRequested) {
            Socket_FlushReadPipe();
        }
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

static KineticPDU PDU;

void test_KineticSocket_Write_should_write_the_data_to_the_specified_socket(void)
{
    LOG_LOCATION;

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    KineticStatus status = KineticSocket_Write(FileDesc, &TestDataBuffer);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SUCCESS, status, "Failed to write to socket!");
    Socket_FlushReadPipe();
}

void test_KineticSocket_WriteProtobuf_should_write_serialized_protobuf_to_the_specified_socket(void)
{
    LOG_LOCATION;
    KineticSession session = {
        .clusterVersion = 12345678,
        .identity = -12345678,
    };
    KineticConnection connection;
    KINETIC_CONNECTION_INIT(&connection);
    connection.session = session;
    KINETIC_PDU_INIT_WITH_COMMAND(&PDU, &connection);
    KineticMessage_Init(&PDU.protoData.message);
    PDU.header.protobufLength = KineticProto_Message__get_packed_size(PDU.proto);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

#if defined(__unix__) && !defined(__APPLE__)
    TEST_IGNORE_MESSAGE("Disabled on Linux until KineticRuby server client connection cleanup is fixed!");
#endif

    LOG("Writing a dummy protobuf...");
    KineticStatus status = KineticSocket_WriteProtobuf(FileDesc, &PDU);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SUCCESS, status, "Failed to write to socket!");
}

void test_KineticSocket_Read_should_read_data_from_the_specified_socket(void)
{
    LOG_LOCATION;
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
    const size_t len = 5;
    uint8_t respData[len];
    ByteBuffer respBuffer = ByteBuffer_Create(respData, sizeof(respData), 0);

    Socket_RequestBytes(len);

    KineticStatus status = KineticSocket_Read(FileDesc, &respBuffer, len);

    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SUCCESS, status, "Failed to read from socket!");
    TEST_ASSERT_EQUAL_MESSAGE(
        len, respBuffer.bytesUsed, "Received incorrect number of bytes");
}

void test_KineticSocket_Read_should_timeout_if_requested_data_is_not_received_within_configured_timeout(void)
{
    LOG_LOCATION;
    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");
    const size_t len = 5;
    uint8_t respData[len + 2];
    ByteBuffer respBuffer = ByteBuffer_Create(respData, sizeof(respData), 0);

    // Send request to test server to send us some data
    Socket_RequestBytes(len);

    // Try to read more than was requested, to cause a timeout
    KineticStatus status = KineticSocket_Read(
                               FileDesc, &respBuffer, sizeof(respData));
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SOCKET_TIMEOUT, status,
        "Expected socket to timeout waiting on data!");
}

void test_KineticSocket_Read_should_read_up_to_the_array_length_into_the_buffer_discard_remainder_and_return_BUFFER_OVERRUN_with_bytesUsed_set_to_the_total_length_read(void)
{
    LOG_LOCATION;
    uint8_t respData[20];
    const size_t bufferLen = sizeof(respData);
    const size_t bytesToRead = bufferLen + 15; // Request more than the size of the allocated array
    ByteBuffer respBuffer = ByteBuffer_Create(respData, bufferLen, 0);

    FileDesc = KineticSocket_Connect("localhost", KineticTestPort, true);
    TEST_ASSERT_TRUE_MESSAGE(FileDesc >= 0, "File descriptor invalid");

    Socket_RequestBytes(bytesToRead);

    KineticStatus status = KineticSocket_Read(FileDesc, &respBuffer, bytesToRead);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_BUFFER_OVERRUN, status,
        "Socket read should have failed due to too many bytes requested!");
    TEST_ASSERT_EQUAL_MESSAGE(
        bytesToRead, respBuffer.bytesUsed,
        "bytesUsed should reflect full length read upon overflow");
}



void test_KineticSocket_ReadProtobuf_should_read_the_specified_length_of_an_encoded_protobuf_from_the_specified_socket(void)
{
    LOG_LOCATION;
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
    Socket_RequestProtobuf();

    // Receive the response
    KINETIC_PDU_INIT(&PDU, &connection);
    PDU.header.protobufLength = 125;
    TEST_ASSERT_FALSE(PDU.protobufDynamicallyExtracted);
    TEST_ASSERT_NULL(PDU.proto);
    KineticStatus status = KineticSocket_ReadProtobuf(FileDesc, &PDU);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(KINETIC_STATUS_SUCCESS, status,
                                            "Failed receiving protobuf response");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        PDU.proto,
        "Protobuf pointer was NULL, but expected dynamic memory allocation!");
    TEST_ASSERT_TRUE_MESSAGE(
        PDU.protobufDynamicallyExtracted,
        "Flag was not set per dynamically allocated/extracted protobuf");

    LOG("Received Kinetic protobuf:");
    LOGF("  command: (0x%zX)", (size_t)PDU.command);
    // LOGF("    header: (0x%zX)", (size_t)PDU.command->header);
    // LOGF("      identity: %016llX",
    //      (unsigned long long)PDU.command->header->identity);
    KineticProto_Message__free_unpacked(PDU.proto, NULL);
    // ByteArray hmacArray = {
    //     .data = PDU.proto->hmac.data, .len = PDU.proto->hmac.len
    // };
    // KineticLogger_LogByteArray("  hmac", hmacArray);

    LOG("Kinetic ProtoBuf read successfully!");
}

void test_KineticSocket_ReadProtobuf_should_return_false_if_KineticProto_of_specified_length_fails_to_be_read_within_timeout(void)
{
    LOG_LOCATION;
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
    KineticStatus status = KineticSocket_Write(FileDesc, &requestBuffer);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(KINETIC_STATUS_SUCCESS, status,
                                            "Failed sending protobuf read request");

    // Receive the dummy protobuf response, but expect too much data
    // to force timeout
    KINETIC_PDU_INIT(&PDU, &connection);
    PDU.header.protobufLength = 1000;
    TEST_ASSERT_FALSE(PDU.protobufDynamicallyExtracted);
    TEST_ASSERT_NULL(PDU.proto);
    status = KineticSocket_ReadProtobuf(FileDesc, &PDU);
    TEST_ASSERT_EQUAL_KineticStatus_MESSAGE(
        KINETIC_STATUS_SOCKET_TIMEOUT, status,
        "Expected socket to timeout waiting on protobuf data!");
    TEST_ASSERT_FALSE_MESSAGE(PDU.protobufDynamicallyExtracted,
                              "Protobuf should not have been extracted because of timeout");
    TEST_ASSERT_NULL_MESSAGE(PDU.proto,
                             "Protobuf should not have been allocated because of timeout");
}

#endif // defined(__APPLE__)
