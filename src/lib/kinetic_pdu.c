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

#include "kinetic_pdu.h"
#include "kinetic_nbo.h"
#include "kinetic_session.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "kinetic_nbo.h"
#include "kinetic_allocator.h"
#include "kinetic_controller.h"
#include "bus.h"
#include "kinetic_pdu_unpack.h"

#ifdef TEST
#define STATIC
#else
#define STATIC static
#endif

#include <time.h>

STATIC void log_cb(log_event_t event, int log_level, const char *msg, void *udata) {
    (void)udata;
    const char *event_str = bus_log_event_str(event);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    FILE *f = stdout; /* stderr */
    fprintf(f, "%ld.%06d: %s[%d] -- %s\n",
        tv.tv_sec, (int)tv.tv_usec,
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

STATIC bool unpack_header(uint8_t const * const read_buf, size_t const read_size, KineticPDUHeader * const header)
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

    if (protobufLength <= PDU_PROTO_MAX_LEN &&
        valueLength <= PDU_PROTO_MAX_LEN) {

        *header = (KineticPDUHeader){
            .versionPrefix = versionPrefix,
            .protobufLength = protobufLength,
            .valueLength = valueLength,
        };
        return true;
    } else {
        return false;
    }
}

STATIC bus_sink_cb_res_t sink_cb(uint8_t *read_buf,
        size_t read_size, void *socket_udata) {

    KineticConnection * connection = (KineticConnection *)socket_udata;
    assert(connection);
    socket_info *si = connection->si;
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

STATIC bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata) {
    (void)socket_udata;
    /* just got .full_msg_buffer from sink_cb -- pass it along as-is */
    socket_info *si = (socket_info *)msg;

    if (si->unpack_status != UNPACK_ERROR_SUCCESS)
    {
        return (bus_unpack_cb_res_t) {
            .ok = false,
            .u.error.opaque_error_id = si->unpack_status,
        };
    }

    KineticResponse * response = KineticAllocator_NewKineticResponse(si->header.valueLength);

    if (response == NULL) {
        bus_unpack_cb_res_t res = {
            .ok = false,
            .u.error.opaque_error_id = UNPACK_ERROR_PAYLOAD_MALLOC_FAIL,
        };
        return res;
    } else {
        response->header = si->header;

        response->proto = KineticPDU_unpack_message(NULL, si->header.protobufLength, si->buf);
        if (response->proto->has_commandBytes &&
            response->proto->commandBytes.data != NULL &&
            response->proto->commandBytes.len > 0)
        {
            response->command = KineticPDU_unpack_command(NULL,
                response->proto->commandBytes.len, response->proto->commandBytes.data);
        } else {
            response->command = NULL;
        }

        if (response->header.valueLength > 0)
        {
            memcpy(response->value, &si->buf[si->header.protobufLength], si->header.valueLength);
        }

        int64_t seq_id = 0;
        if (response->command != NULL &&
            response->command->header != NULL) {

            seq_id = response->command->header->ackSequence;
        }

        bus_unpack_cb_res_t res = {
            .ok = true,
            .u.success = {
                .seq_id = seq_id,
                .msg = response,
            },
        };
        return res;
    }
}

bool KineticPDU_InitBus(int log_level, KineticClient * client)
{
    bus_config cfg = {
        .log_cb = log_cb,
        .log_level = log_level,
        .sink_cb = sink_cb,
        .unpack_cb = unpack_cb,
        .unexpected_msg_cb = KineticController_HandleUnexecpectedResponse,
        .bus_udata = NULL,
        .sender_count = 4,
        .listener_count = 4,
    };
    bus_result res = {0};
    if (!bus_init(&cfg, &res)) {
        LOGF0("failed to init bus: %d\n", res.status);
        return false;
    }
    client->bus = res.bus;
    return true;
}

void KineticPDU_DeinitBus(KineticClient * const client)
{
    bus_shutdown(client->bus);
    bus_free(client->bus);
}

KineticStatus KineticPDU_GetStatus(KineticResponse* response)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (response != NULL &&
        response->command != NULL &&
        response->command->status != NULL &&
        response->command->status->has_code != false)
    {
        status = KineticProtoStatusCode_to_KineticStatus(
            response->command->status->code);
    }

    return status;
}

KineticProto_Command_KeyValue* KineticPDU_GetKeyValue(KineticResponse* response)
{
    KineticProto_Command_KeyValue* keyValue = NULL;

    if (response != NULL &&
        response->command != NULL &&
        response->command->body != NULL)
    {
        keyValue = response->command->body->keyValue;
    }
    return keyValue;
}

KineticProto_Command_Range* KineticPDU_GetKeyRange(KineticResponse* response)
{
    KineticProto_Command_Range* range = NULL;
    if (response != NULL &&
        response->proto != NULL &&
        response->command != NULL &&
        response->command->body != NULL)
    {
        range = response->command->body->range;
    }
    return range;
}
