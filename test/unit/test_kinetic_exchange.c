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
#include "kinetic_proto.h"
#include "mock_kinetic_connection.h"
#include "kinetic_exchange.h"
#include <protobuf-c/protobuf-c.h>
#include <time.h>

static KineticConnection Connection;
static KineticExchange Exchange;
static int64_t Identity = 1234;
static char Key[KINETIC_MAX_KEY_LEN];
static size_t KeyLength = 0;
static int64_t ConnectionID = 5678;

void setUp(void)
{
    size_t i;
    memset(Key, 0, sizeof(Key));
    KeyLength = 17;
    for (i = 0; i < KeyLength; i++)
    {
        Key[i] = (uint8_t)(0x0FFu & i);
    }
    Key[i] = '\0';

    KineticExchange_Init(&Exchange, Identity, Key, KeyLength, &Connection);
}

void tearDown(void)
{
}

void test_KineticExchange_Init_should_initialize_a_KineticExchange(void)
{
    TEST_ASSERT_EQUAL_PTR(&Connection, Exchange.connection);
    TEST_ASSERT_FALSE(Exchange.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1234, Exchange.identity);
    TEST_ASSERT_TRUE(Exchange.has_key);
    TEST_ASSERT_EQUAL_UINT64(KeyLength, Exchange.keyLength);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(Key, Exchange.key, KeyLength);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.connectionID);
    TEST_ASSERT_EQUAL_INT64(-1, Exchange.sequence);
}

void test_KineticExchange_Init_should_initialize_a_KineticExchange_without_a_key(void)
{
    memset(Key, 0, sizeof(Key));

    KineticExchange_Init(&Exchange, Identity, NULL, 1823642323, &Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, Exchange.connection);
    TEST_ASSERT_FALSE(Exchange.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1234, Exchange.identity);
    TEST_ASSERT_FALSE(Exchange.has_key);
    TEST_ASSERT_EQUAL_UINT64(0, Exchange.keyLength);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.connectionID);
    TEST_ASSERT_EQUAL_INT64(-1, Exchange.sequence);
    KineticExchange_Init(&Exchange, Identity, NULL, 1823642323, &Connection);

    KineticExchange_Init(&Exchange, Identity, Key, 0, &Connection);

    TEST_ASSERT_EQUAL_PTR(&Connection, Exchange.connection);
    TEST_ASSERT_FALSE(Exchange.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1234, Exchange.identity);
    TEST_ASSERT_FALSE(Exchange.has_key);
    TEST_ASSERT_EQUAL_UINT64(0, Exchange.keyLength);
    TEST_ASSERT_EQUAL_INT64(0, Exchange.connectionID);
    TEST_ASSERT_EQUAL_INT64(-1, Exchange.sequence);
}

void test_KineticExchange_ConfigureConnectionID_should_set_ID_to_seconds_since_epoch_from_now(void)
{
    time_t cur_time;
    cur_time = time(NULL);

    KineticExchange_ConfigureConnectionID(&Exchange);

    // Give 1-second flexibility in the rare case that
    // we were on a second boundary
    TEST_ASSERT_TRUE(Exchange.connectionID >= cur_time);
    TEST_ASSERT_TRUE(Exchange.connectionID <= cur_time + 1);
}

void test_KineticExchange_SetClusterVersion_should_set_and_enable_the_field(void)
{
    KineticExchange_SetClusterVersion(&Exchange, 1122334455667788);

    TEST_ASSERT_TRUE(Exchange.has_clusterVersion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788, Exchange.clusterVersion);
}

void test_KineticExchange_IncrementSequence_should_set_and_enable_the_field(void)
{
    Exchange.sequence = 57;

    KineticExchange_IncrementSequence(&Exchange);

    TEST_ASSERT_EQUAL_INT64(58, Exchange.sequence);
}

void test_KineticExchange_ConfigureHeader_should_set_the_exchange_fields_in_the_specified_header(void)
{
    KineticProto_Header header = KINETIC_PROTO_HEADER_INIT;
    Exchange.sequence = 24;
    KineticExchange_SetClusterVersion(&Exchange, 1122334455667788);

    KineticExchange_ConfigureHeader(&Exchange, &header);

    TEST_ASSERT_TRUE(header.has_clusterversion);
    TEST_ASSERT_EQUAL_INT64(1122334455667788, header.clusterversion);
    TEST_ASSERT_TRUE(header.has_identity);
    TEST_ASSERT_EQUAL_INT64(1234, header.identity);
    TEST_ASSERT_TRUE(header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(0, header.connectionid);
    TEST_ASSERT_TRUE(header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(24, header.sequence);
}

void test_KineticExchange_ConfigureHeader_should_not_set_nor_enable_the_exchangeid_field_in_the_specified_header_if_not_configured(void)
{
    KineticProto_Header header = KINETIC_PROTO_HEADER_INIT;

    KineticExchange_IncrementSequence(&Exchange);
    KineticExchange_ConfigureHeader(&Exchange, &header);

    TEST_ASSERT_FALSE(header.has_clusterversion);
    TEST_ASSERT_TRUE(header.has_identity);
    TEST_ASSERT_EQUAL_INT64(1234, header.identity);
    TEST_ASSERT_TRUE(header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(0, header.connectionid);
    TEST_ASSERT_TRUE(header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(0, header.sequence);

    KineticExchange_IncrementSequence(&Exchange);
    KineticExchange_ConfigureHeader(&Exchange, &header);

    TEST_ASSERT_FALSE(header.has_clusterversion);
    TEST_ASSERT_TRUE(header.has_identity);
    TEST_ASSERT_EQUAL_INT64(1234, header.identity);
    TEST_ASSERT_TRUE(header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(0, header.connectionid);
    TEST_ASSERT_TRUE(header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(1, header.sequence);

    Exchange.sequence = 57;
    KineticExchange_IncrementSequence(&Exchange);
    KineticExchange_ConfigureHeader(&Exchange, &header);

    TEST_ASSERT_FALSE(header.has_clusterversion);
    TEST_ASSERT_TRUE(header.has_identity);
    TEST_ASSERT_EQUAL_INT64(1234, header.identity);
    TEST_ASSERT_TRUE(header.has_connectionid);
    TEST_ASSERT_EQUAL_INT64(0, header.connectionid);
    TEST_ASSERT_TRUE(header.has_sequence);
    TEST_ASSERT_EQUAL_INT64(58, header.sequence);
}
