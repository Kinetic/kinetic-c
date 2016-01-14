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

#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"
#include "system_test_fixture.h"
#include "protobuf-c/protobuf-c.h"
#include "socket99.h"
#include <string.h>
#include <stdlib.h>

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_device_info.h"
#include "kinetic.pb-c.h"
#include "kinetic_allocator.h"
#include "kinetic_message.h"
#include "kinetic_bus.h"
#include "kinetic_logger.h"
#include "kinetic_operation.h"
#include "kinetic_hmac.h"
#include "kinetic_socket.h"
#include "kinetic_nbo.h"

#include "bus.h"

static void log_cb(log_event_t event, int log_level, const char *msg, void *udata) {
    (void)udata;
    const char *event_str = Bus_LogEventStr(event);
    fprintf(stderr, "%s[%d] -- %s\n",
        event_str, log_level, msg);
}

static bus_sink_cb_res_t reset_transfer(socket_info *si) {
    bus_sink_cb_res_t res = { /* prime pump with header size */
        .next_read = sizeof(KineticPDUHeader),
    };

    si->state = STATE_AWAITING_HEADER;
    si->accumulated = 0;
    si->unpack_status = UNPACK_ERROR_UNDEFINED;
    memset(&si->header, 0x00, sizeof(si->header));
    return res;
}


static bool unpack_header(uint8_t const * const read_buf, size_t const read_size, KineticPDUHeader * const header)
{
    if (read_size != sizeof(KineticPDUHeader)) {
        return false;
        // TODO this will fail if we don't get all of the header bytes in one read
        // we should fix this
    }
    KineticPDUHeader const * const buf_header = (KineticPDUHeader const * const)read_buf;
    uint32_t protobufLength = KineticNBO_ToHostU32(buf_header->protobufLength);
    uint32_t valueLength = KineticNBO_ToHostU32(buf_header->valueLength);
    uint8_t versionPrefix = buf_header->versionPrefix;

    *header = (KineticPDUHeader){
        .versionPrefix = versionPrefix,
        .protobufLength = protobufLength,
        .valueLength = valueLength,
    };
    if (header->protobufLength <= PDU_PROTO_MAX_LEN &&
        header->valueLength <= PDU_PROTO_MAX_LEN) {
        return true;
    } else {
        return false;
    }
}

static bus_sink_cb_res_t sink_cb(uint8_t *read_buf, size_t read_size, void *socket_udata)
{

    socket_info *si = (socket_info *)socket_udata;
    assert(si);

    switch (si->state) {
    case STATE_UNINIT:
    {
        assert(read_size == 0);
        return reset_transfer(si);
    }
    case STATE_AWAITING_HEADER:
    {
        if (unpack_header(read_buf, read_size, &si->header))
        {
            si->accumulated = 0;
            si->unpack_status = UNPACK_ERROR_SUCCESS;
            si->state = STATE_AWAITING_BODY;
            bus_sink_cb_res_t res = {
                .next_read = si->header.protobufLength + si->header.valueLength,
            };
            return res;
        } else {
            si->accumulated = 0;
            si->unpack_status = UNPACK_ERROR_INVALID_HEADER;
            si->state = STATE_AWAITING_HEADER;
            bus_sink_cb_res_t res = {
                .next_read = sizeof(KineticPDUHeader),
                .full_msg_buffer = si,
            };
            return res;
        }
        break;
    }
    case STATE_AWAITING_BODY:
    {
        memcpy(&si->buf[si->accumulated], read_buf, read_size);
        si->accumulated += read_size;

        uint32_t remaining = si->header.protobufLength + si->header.valueLength - si->accumulated;

        if (remaining == 0) {
            si->state = STATE_AWAITING_HEADER;
            bus_sink_cb_res_t res = {
                .next_read = sizeof(KineticPDUHeader),
                // returning the whole si, because we need access to the pdu header as well
                //  as the protobuf and value bytes
                .full_msg_buffer = si,
            };
            return res;
        } else {
            bus_sink_cb_res_t res = {
                .next_read = remaining,
            };
            return res;
        }
        break;
    }
    default:
        assert(false);
    }
}

struct kinetic_response_payload
{
    KineticPDUHeader header;
    Com__Seagate__Kinetic__Proto__Message* protoBuf;
    Com__Seagate__Kinetic__Proto__Command* command;
    uint8_t value[];
};

static void free_payload(struct kinetic_response_payload * payload)
{
    if (payload != NULL) {
        if (payload->command) {
            protobuf_c_message_free_unpacked(&payload->command->base, NULL);
        }
        if (payload->protoBuf) {
            protobuf_c_message_free_unpacked(&payload->protoBuf->base, NULL);
        }
        free(payload);
    }
}

static bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata) {
    /* just got .full_msg_buffer from sink_cb -- pass it along as-is */
    socket_info *si = (socket_info *)msg;
    assert(socket_udata == si); //These don't have to be equal, but in this case they will be.

    if (si->unpack_status != UNPACK_ERROR_SUCCESS)
    {
        return (bus_unpack_cb_res_t) {
            .ok = false,
            .u.error.opaque_error_id = si->unpack_status,
        };
    }

    struct kinetic_response_payload * payload = malloc(sizeof(*payload) + si->header.valueLength);

    if (payload == NULL) {
        bus_unpack_cb_res_t res = {
            .ok = false,
            .u.error.opaque_error_id = UNPACK_ERROR_PAYLOAD_MALLOC_FAIL,
        };
        return res;
    } else {
        payload->header = si->header;
        payload->protoBuf = com__seagate__kinetic__proto__message__unpack(NULL,
            si->header.protobufLength, si->buf);
        if (payload->protoBuf->has_commandbytes &&
            payload->protoBuf->commandbytes.data != NULL &&
            payload->protoBuf->commandbytes.len > 0)
        {
            payload->command = com__seagate__kinetic__proto__command__unpack(NULL,
                payload->protoBuf->commandbytes.len, payload->protoBuf->commandbytes.data);
        } else {
            payload->command = NULL;
        }

        if (payload->header.valueLength > 0)
        {
            memcpy(payload->value, &si->buf[si->header.protobufLength], si->header.valueLength);
        }

        bus_unpack_cb_res_t res = {
            .ok = true,
            .u.success = {
                .seq_id = payload->command->header->acksequence,
                .msg = payload,
            },
        };
        return res;
    }
}

static void unexpected_msg_cb(void *msg,
        int64_t seq_id, void *bus_udata, void *socket_udata) {
    struct kinetic_response_payload * payload = (struct kinetic_response_payload *)msg;

    KineticLogger_LogProtobuf(3, payload->protoBuf);

    printf("\n\n\nUNEXPECTED MESSAGE: %p, seq_id %lld, bus_udata %p, socket_udata %p\n\n\n\n",
        msg, (long long)seq_id, bus_udata, socket_udata);

    free_payload(payload);
}


void test_that_we_can_register_sockets(void)
{

    KineticLogger_Init("stdout", 2);
    bus_config cfg = {
        .log_cb = log_cb,
        .log_level = /*2,*/ 5,
        .sink_cb = sink_cb,
        .unpack_cb = unpack_cb,
        .unexpected_msg_cb = unexpected_msg_cb,
        .bus_udata = NULL,
    };
    bus_result res = {0};
    if (!Bus_Init(&cfg, &res)) {
        LOGF1(0, "failed to init bus: %d\n", res.status);
        return;
    }

    socket_info * si = calloc(1, sizeof(socket_info) + 2 * PDU_PROTO_MAX_LEN);
    assert(si != NULL);

    int fd = KineticSocket_Connect(GetSystemTestHost1(), GetSystemTestPort1());
    assert(fd != KINETIC_SOCKET_DESCRIPTOR_INVALID);
    bool result = Bus_RegisterSocket(res.bus, BUS_SOCKET_PLAIN, fd, si);
    assert(result);

    sleep(5);

    Bus_Shutdown(res.bus);
    Bus_Free(res.bus);

    free(si);

    KineticSocket_Close(fd);

    KineticLogger_Close();
}

void test_that_we_can_register_SSL_sockets(void)
{

    KineticLogger_Init("stdout", 3);
    bus_config cfg = {
        .log_cb = log_cb,
        .log_level = /*2,*/ 5,
        .sink_cb = sink_cb,
        .unpack_cb = unpack_cb,
        .unexpected_msg_cb = unexpected_msg_cb,
        .bus_udata = NULL,
    };
    bus_result res = {0};
    if (!Bus_Init(&cfg, &res)) {
        LOGF1(0, "failed to init bus: %d\n", res.status);
        return;
    }

    socket_info * si = calloc(1, sizeof(socket_info) + 2 * PDU_PROTO_MAX_LEN);
    assert(si != NULL);

    int fd = KineticSocket_Connect(GetSystemTestHost1(), GetSystemTestTlsPort1());
    assert(fd != KINETIC_SOCKET_DESCRIPTOR_INVALID);
    bool result = Bus_RegisterSocket(res.bus, BUS_SOCKET_SSL, fd, si);
    assert(result);

    sleep(5);

    Bus_Shutdown(res.bus);
    Bus_Free(res.bus);

    free(si);

    KineticSocket_Close(fd);

    KineticLogger_Close();
}
