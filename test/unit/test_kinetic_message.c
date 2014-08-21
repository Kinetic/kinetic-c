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

#include "unity.h"
#include "unity_helper.h"
#include "protobuf-c.h"
#include "kinetic_types.h"
#include "kinetic_proto.h"
#include "kinetic_message.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KineticMessage_Init_should_initialize_the_message_and_required_protobuf_fields(void)
{
    KineticMessage message;

    KineticMessage_Init(&message);

    TEST_ASSERT_EQUAL_PTR(&message.header, message.command.header);
    TEST_ASSERT_EQUAL_PTR(&message.command, message.proto.command);
    TEST_ASSERT_TRUE(message.proto.has_hmac);
    TEST_ASSERT_EQUAL_PTR(message.hmacData, message.proto.hmac.data);
    TEST_ASSERT_EQUAL(KINETIC_HMAC_MAX_LEN, message.proto.hmac.len);
    TEST_ASSERT_NULL(message.command.body);
    TEST_ASSERT_NULL(message.command.status);
}

void test_KineticMessage_ConfigureKeyValue_should_configure_Body_KeyValue_and_add_to_message(void)
{
    KineticMessage message;
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    ByteArray newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v2.0");
    ByteArray dbVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");

    KineticMessage_Init(&message);

    // struct _KineticProto_KeyValue
    // {
    //     ProtobufCMessage base;
    //     protobuf_c_boolean has_newversion;
    //     ProtobufCBinaryData newversion;
    //     protobuf_c_boolean has_force;
    //     protobuf_c_boolean force;
    //     protobuf_c_boolean has_key;
    //     ProtobufCBinaryData key;
    //     protobuf_c_boolean has_dbversion;
    //     ProtobufCBinaryData dbversion;
    //     protobuf_c_boolean has_tag;
    //     ProtobufCBinaryData tag;
    //     protobuf_c_boolean has_algorithm;
    //     KineticProto_Algorithm algorithm;
    //     protobuf_c_boolean has_metadataonly;
    //     protobuf_c_boolean metadataonly;
    //     protobuf_c_boolean has_synchronization;
    //     KineticProto_Synchronization synchronization;
    // };

    KineticMessage_ConfigureKeyValue(&message, key, newVersion, dbVersion, tag, false);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.body, message.proto.command->body);
    TEST_ASSERT_EQUAL_PTR(&message.keyValue, message.proto.command->body->keyvalue);

    // Validate keyValue fields
    TEST_ASSERT_TRUE(message.keyValue.has_newversion);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(newVersion, message.keyValue.newversion);
    TEST_ASSERT_TRUE(message.keyValue.has_key);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(key, message.keyValue.key);
    TEST_ASSERT_TRUE(message.keyValue.has_dbversion);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(dbVersion, message.keyValue.dbversion);
    TEST_ASSERT_TRUE(message.keyValue.has_tag);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(tag, message.keyValue.tag);
    TEST_ASSERT_FALSE(message.keyValue.has_metadataonly);
    TEST_ASSERT_FALSE(message.keyValue.metadataonly);

    // Not implemented as of (8/13/2014)
    TEST_ASSERT_FALSE(message.keyValue.has_force);
    TEST_ASSERT_FALSE(message.keyValue.has_algorithm);
    TEST_ASSERT_FALSE(message.keyValue.has_synchronization);
}

void test_KineticMessage_ConfigureKeyValue_should_configure_Body_KeyValue_for_metadata_only_and_add_to_message(void)
{
    KineticMessage message;
    ByteArray key = BYTE_ARRAY_INIT_FROM_CSTRING("my_key_3.1415927");
    ByteArray newVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v2.0");
    ByteArray dbVersion = BYTE_ARRAY_INIT_FROM_CSTRING("v1.0");
    ByteArray tag = BYTE_ARRAY_INIT_FROM_CSTRING("SomeTagValue");

    KineticMessage_Init(&message);

    // struct _KineticProto_KeyValue
    // {
    //     ProtobufCMessage base;
    //     protobuf_c_boolean has_newversion;
    //     ProtobufCBinaryData newversion;
    //     protobuf_c_boolean has_force;
    //     protobuf_c_boolean force;
    //     protobuf_c_boolean has_key;
    //     ProtobufCBinaryData key;
    //     protobuf_c_boolean has_dbversion;
    //     ProtobufCBinaryData dbversion;
    //     protobuf_c_boolean has_tag;
    //     ProtobufCBinaryData tag;
    //     protobuf_c_boolean has_algorithm;
    //     KineticProto_Algorithm algorithm;
    //     protobuf_c_boolean has_metadataonly;
    //     protobuf_c_boolean metadataonly;
    //     protobuf_c_boolean has_synchronization;
    //     KineticProto_Synchronization synchronization;
    // };

    KineticMessage_ConfigureKeyValue(&message, key, newVersion, dbVersion, tag, true);

    // Validate that message keyValue and body container are enabled in protobuf
    TEST_ASSERT_EQUAL_PTR(&message.body, message.command.body);
    TEST_ASSERT_EQUAL_PTR(&message.body, message.proto.command->body);
    TEST_ASSERT_EQUAL_PTR(&message.keyValue, message.proto.command->body->keyvalue);

    // Validate keyValue fields
    TEST_ASSERT_TRUE(message.keyValue.has_newversion);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(newVersion, message.keyValue.newversion);
    TEST_ASSERT_TRUE(message.keyValue.has_key);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(key, message.keyValue.key);
    TEST_ASSERT_TRUE(message.keyValue.has_dbversion);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(dbVersion, message.keyValue.dbversion);
    TEST_ASSERT_TRUE(message.keyValue.has_tag);
    TEST_ASSERT_EQUAL_BYTE_ARRAY(tag, message.keyValue.tag);
    TEST_ASSERT_TRUE(message.keyValue.has_metadataonly);
    TEST_ASSERT_TRUE(message.keyValue.metadataonly);

    // Not implemented as of (8/13/2014)
    TEST_ASSERT_FALSE(message.keyValue.has_force);
    TEST_ASSERT_FALSE(message.keyValue.has_algorithm);
    TEST_ASSERT_FALSE(message.keyValue.has_synchronization);
}
