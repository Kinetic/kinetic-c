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

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_bus.h"
#include "kinetic_nbo.h"
#include "kinetic.pb-c.h"
#include "kinetic_logger.h"
#include "mock_kinetic_session.h"
#include "mock_kinetic_message.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_hmac.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_pdu_unpack.h"
#include "mock_bus.h"
#include "mock_bus_inward.h"
#include "byte_array.h"
#include "protobuf-c/protobuf-c.h"

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

static uint32_t ClusterVersion = 7;
static KineticRequest Request;
static KineticResponse Response;
static KineticSession Session;
static uint8_t ValueBuffer[KINETIC_OBJ_SIZE];
static ByteArray Value = {.data = ValueBuffer, .len = sizeof(ValueBuffer)};

#define SI_BUF_SIZE (sizeof(socket_info) + 2 * PDU_PROTO_MAX_LEN)
static uint8_t si_buf[SI_BUF_SIZE];

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
    Session = (KineticSession) {
        .connected = true,
        .socket = 456,
        .config = (KineticSessionConfig) {
            .port = 1234,
            .host = "valid-host.com",
            .hmacKey = ByteArray_CreateWithCString("some valid HMAC key..."),
            .clusterVersion = ClusterVersion,
        }
    };
    KineticRequest_Init(&Request, &Session);
    ByteArray_FillWithDummyData(Value);

    memset(si_buf, 0, SI_BUF_SIZE);
}

void tearDown(void)
{
    memset(&Response, 0, sizeof(Response));
    KineticLogger_Close();
}

bool unpack_header(uint8_t const * const read_buf,
    size_t const read_size, KineticPDUHeader * const header);

void test_unpack_header_should_fail_if_the_header_is_the_wrong_size(void)
{
    KineticPDUHeader header = {0};
    KineticPDUHeader header_out = {0};
    TEST_ASSERT_FALSE(unpack_header((uint8_t *)&header, sizeof(header) - 1, &header_out));
    TEST_ASSERT_FALSE(unpack_header((uint8_t *)&header, sizeof(header) + 1, &header_out));
}

void test_unpack_header_should_reject_header_with_excessively_large_sizes(void)
{
    KineticPDUHeader header = {0};
    KineticPDUHeader header_out = {0};

    header.protobufLength = PDU_PROTO_MAX_LEN + 1;
    TEST_ASSERT_FALSE(unpack_header((uint8_t *)&header, sizeof(header), &header_out));

    header.protobufLength = PDU_PROTO_MAX_LEN;
    header.valueLength = PDU_PROTO_MAX_LEN + 1;
    TEST_ASSERT_FALSE(unpack_header((uint8_t *)&header, sizeof(header), &header_out));
}

void test_unpack_header_should_unpack_header_fields_from_read_buf(void)
{
    uint8_t read_buf[] = {
        0xa0,                       // version prefix
        0x00, 0x01, 0x23, 0x45,     // protobuf length
        0x00, 0x02, 0x34, 0x56,     // value length
    };
    KineticPDUHeader header_out = {0};

    TEST_ASSERT(unpack_header(read_buf, sizeof(header_out), &header_out));
    TEST_ASSERT_EQUAL(0xa0, header_out.versionPrefix);
    TEST_ASSERT_EQUAL(0x12345, header_out.protobufLength);
    TEST_ASSERT_EQUAL(0x23456, header_out.valueLength);
}

bus_sink_cb_res_t sink_cb(uint8_t *read_buf,
    size_t read_size, void *socket_udata);

void test_sink_cb_should_reset_uninitialized_socket_state(void)
{
    socket_info *si = (socket_info *)si_buf;
    *si = (socket_info){
        .state = STATE_UNINIT,
        .accumulated = 0xFFFFFFFF,
    };
    Session.si = si;

    bus_sink_cb_res_t res = sink_cb(NULL, 0, &Session);

    TEST_ASSERT_EQUAL(0, si->accumulated);
    TEST_ASSERT_EQUAL(UNPACK_ERROR_UNDEFINED, si->unpack_status);

    /* Expect next read to be a header. */
    TEST_ASSERT_EQUAL(sizeof(KineticPDUHeader), res.next_read);
    TEST_ASSERT_EQUAL(NULL, res.full_msg_buffer);
    TEST_ASSERT_EQUAL(STATE_AWAITING_HEADER, si->state);
}

void test_sink_cb_should_expose_invalid_header_error(void)
{
    socket_info *si = (socket_info *)si_buf;
    *si = (socket_info){
        .state = STATE_AWAITING_HEADER,
        .accumulated = 0,
    };
    Session.si = si;
    KineticPDUHeader bad_header;
    memset(&bad_header, 0xFF, sizeof(bad_header));

    bus_sink_cb_res_t res = sink_cb((uint8_t *)&bad_header, sizeof(bad_header), &Session);
    TEST_ASSERT_EQUAL(sizeof(KineticPDUHeader), res.next_read);
    TEST_ASSERT_EQUAL(UNPACK_ERROR_INVALID_HEADER, si->unpack_status);
    TEST_ASSERT_EQUAL(si, res.full_msg_buffer);

    TEST_ASSERT_EQUAL(0, si->accumulated);
    TEST_ASSERT_EQUAL(STATE_AWAITING_HEADER, si->state);
}

void test_sink_cb_should_transition_to_awaiting_body_state_with_good_header(void)
{
    socket_info *si = (socket_info *)si_buf;
    *si = (socket_info){
        .state = STATE_AWAITING_HEADER,
        .accumulated = 0,
    };
    Session.si = si;
    uint8_t read_buf[] = {
        0xa0,                       // version prefix
        0x00, 0x00, 0x00, 0x7b,     // protobuf length
        0x00, 0x00, 0x01, 0xc8,     // value length
    };

    bus_sink_cb_res_t res = sink_cb(read_buf, sizeof(read_buf), &Session);
    TEST_ASSERT_EQUAL(123 + 456, res.next_read);
    TEST_ASSERT_EQUAL(NULL, res.full_msg_buffer);
    TEST_ASSERT_EQUAL(STATE_AWAITING_BODY, si->state);
    TEST_ASSERT_EQUAL(0, si->accumulated);
    TEST_ASSERT_EQUAL(UNPACK_ERROR_SUCCESS, si->unpack_status);
}

void test_sink_cb_should_accumulate_partially_recieved_header(void)
{
    socket_info *si = (socket_info *)si_buf;
    *si = (socket_info){
        .state = STATE_AWAITING_HEADER,
        .accumulated = 0,
    };
    Session.si = si;
    uint8_t read_buf1[] = {
        0xa0,                       // version prefix
        0x00, 0x00, 0x00, 0x7b,     // protobuf length
    };
    uint8_t read_buf2[] = {
        0x00, 0x00, 0x01, 0xc8,     // value length
    };

    bus_sink_cb_res_t res = sink_cb(read_buf1, sizeof(read_buf1), &Session);
    TEST_ASSERT_EQUAL(4, res.next_read);
    TEST_ASSERT_EQUAL(STATE_AWAITING_HEADER, si->state);
    TEST_ASSERT_EQUAL(5, si->accumulated);

    res = sink_cb(read_buf2, sizeof(read_buf2), &Session);

    TEST_ASSERT_EQUAL(123 + 456, res.next_read);
    TEST_ASSERT_EQUAL(NULL, res.full_msg_buffer);
    TEST_ASSERT_EQUAL(STATE_AWAITING_BODY, si->state);
    TEST_ASSERT_EQUAL(0, si->accumulated);
    TEST_ASSERT_EQUAL(UNPACK_ERROR_SUCCESS, si->unpack_status);
}

void test_sink_cb_should_accumulate_partially_received_body(void)
{
    /* Include trailing memory for si's .buf[]. */
    socket_info *si = (socket_info *)si_buf;

    si->state = STATE_AWAITING_BODY;
    si->header.protobufLength = 0x01;
    si->header.valueLength = 0x02;
    Session.si = si;
    uint8_t buf[] = {0xaa, 0xbb};

    bus_sink_cb_res_t res = sink_cb((uint8_t *)&buf, sizeof(buf), &Session);

    TEST_ASSERT_EQUAL(STATE_AWAITING_BODY, si->state);
    TEST_ASSERT_EQUAL(2, si->accumulated);
    TEST_ASSERT_EQUAL(1, res.next_read);
    TEST_ASSERT_EQUAL(NULL, res.full_msg_buffer);
}

void test_sink_cb_should_yield_fully_received_body(void)
{
    socket_info *si = (socket_info *)si_buf;
    si->state = STATE_AWAITING_BODY;
    si->header.protobufLength = 0x01;
    si->header.valueLength = 0x02;
    Session.si = si;
    uint8_t buf[] = {0xaa, 0xbb, 0xcc};

    bus_sink_cb_res_t res = sink_cb((uint8_t *)&buf, sizeof(buf), &Session);
    TEST_ASSERT_EQUAL(STATE_AWAITING_HEADER, si->state);
    TEST_ASSERT_EQUAL(sizeof(KineticPDUHeader), res.next_read);
    TEST_ASSERT_EQUAL(si, res.full_msg_buffer);
}

bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata);

void test_unpack_cb_should_expose_error_codes(void)
{
    Session.socket = 123;
    socket_info *si = (socket_info *)si_buf;
    *si = (socket_info){
        .state = STATE_AWAITING_HEADER,
        .accumulated = 0,
        .unpack_status = UNPACK_ERROR_UNDEFINED,
    };

    enum unpack_error error_states[] = {
        UNPACK_ERROR_UNDEFINED,
        UNPACK_ERROR_INVALID_HEADER,
        UNPACK_ERROR_PAYLOAD_MALLOC_FAIL,
    };

    for (size_t i = 0; i < sizeof(error_states) / sizeof(error_states[0]); i++) {
        si->unpack_status = error_states[i];
        bus_unpack_cb_res_t res = unpack_cb((void *)si, &Session);
        TEST_ASSERT_FALSE(res.ok);
        TEST_ASSERT_EQUAL(error_states[i], res.u.error.opaque_error_id);
    }
}

void test_unpack_cb_should_expose_alloc_failure(void)
{
    Session.socket = 123;
    socket_info si = {
        .state = STATE_AWAITING_HEADER,
        .accumulated = 0,
        .unpack_status = UNPACK_ERROR_SUCCESS,
        .header = {
            .valueLength = 8,
        },
    };

    KineticAllocator_NewKineticResponse_ExpectAndReturn(8, NULL);
    bus_unpack_cb_res_t res = unpack_cb((void*)&si, &Session);
    TEST_ASSERT_FALSE(res.ok);
    TEST_ASSERT_EQUAL(UNPACK_ERROR_PAYLOAD_MALLOC_FAIL, res.u.error.opaque_error_id);
}

void test_unpack_cb_should_skip_empty_commands(void)
{
    /* Include trailing memory for si's .buf[]. */
    Session.socket = 123;
    socket_info *si = (socket_info *)si_buf;
    si->state = STATE_AWAITING_HEADER;
    si->unpack_status = UNPACK_ERROR_SUCCESS,
    si->header.protobufLength = 0x01;
    si->header.valueLength = 0x01;
    si->header.protobufLength = 0x01;
    si->buf[0] = 0x00;
    si->buf[1] = 0xee;

    uint8_t response_buf[sizeof(KineticResponse) + 128];
    memset(response_buf, 0, sizeof(response_buf));
    KineticResponse *response = (KineticResponse *)response_buf;

    KineticAllocator_NewKineticResponse_ExpectAndReturn(1, response);

    Com__Seagate__Kinetic__Proto__Message Proto;
    memset(&Proto, 0, sizeof(Proto));
    Proto.has_commandbytes = false;

    KineticPDU_unpack_message_ExpectAndReturn(NULL, si->header.protobufLength,
        si->buf, &Proto);

    bus_unpack_cb_res_t res = unpack_cb(si, &Session);

    TEST_ASSERT_EQUAL(0xee, response->value[0]);

    TEST_ASSERT(res.ok);
    TEST_ASSERT_EQUAL(response, res.u.success.msg);
}

void test_unpack_cb_should_unpack_command_bytes(void)
{
    Session.socket = 123;
    socket_info *si = (socket_info *)si_buf;
    si->state = STATE_AWAITING_HEADER;
    si->unpack_status = UNPACK_ERROR_SUCCESS,
    si->header.protobufLength = 0x01;
    si->header.valueLength = 0x08;
    si->header.protobufLength = 0x02;
    si->buf[0] = 0x00;
    si->buf[1] = 0x01;
    si->buf[2] = 0xee;

    uint8_t response_buf[sizeof(KineticResponse) + 1];
    memset(response_buf, 0, sizeof(response_buf));
    KineticResponse *response = (KineticResponse *)response_buf;

    KineticAllocator_NewKineticResponse_ExpectAndReturn(8, response);

    Com__Seagate__Kinetic__Proto__Message Proto;
    memset(&Proto, 0, sizeof(Proto));
    Proto.has_commandbytes = true;
    Proto.commandbytes.data = (uint8_t *)"data";
    Proto.commandbytes.len = 4;

    KineticPDU_unpack_message_ExpectAndReturn(NULL, si->header.protobufLength,
        si->buf, &Proto);

    Com__Seagate__Kinetic__Proto__Command Command;
    memset(&Command, 0, sizeof(Command));
    Com__Seagate__Kinetic__Proto__Command__Header Header;
    memset(&Header, 0, sizeof(Header));
    Command.header = &Header;
    response->header.valueLength = 1;
    Header.acksequence = 0x12345678;

    KineticPDU_unpack_command_ExpectAndReturn(NULL, Proto.commandbytes.len,
        Proto.commandbytes.data, &Command);

    bus_unpack_cb_res_t res = unpack_cb(si, &Session);

    TEST_ASSERT_EQUAL(0xee, response->value[0]);

    TEST_ASSERT(res.ok);
    TEST_ASSERT_EQUAL(response, res.u.success.msg);
    TEST_ASSERT_EQUAL(0x12345678, res.u.success.seq_id);
}
