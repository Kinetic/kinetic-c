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

#include "kinetic_bus.h"
#include "kinetic_nbo.h"
#include "kinetic_session.h"
#include "kinetic_socket.h"
#include "kinetic_hmac.h"
#include "kinetic_logger.h"
#include "kinetic.pb-c.h"
#include "kinetic_nbo.h"
#include "kinetic_allocator.h"
#include "kinetic_controller.h"
#include "bus.h"
#include "kinetic_pdu_unpack.h"

#include <time.h>

STATIC void log_cb(log_event_t event, int log_level, const char *msg, void *udata) {
    (void)udata;
    const char *event_str = Bus_LogEventStr(event);
    KineticLogger_LogPrintf(log_level, "%s[%d] %s", event_str, log_level, msg);
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
        valueLength <= PDU_PROTO_MAX_LEN)
    {
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
        size_t read_size, void *socket_udata)
{
    KineticSession * session = (KineticSession*)socket_udata;
    KINETIC_ASSERT(session);
    socket_info *si = session->si;
    KINETIC_ASSERT(si);

    switch (si->state) {
    case STATE_UNINIT:
    {
        KINETIC_ASSERT(read_size == 0);
        return reset_transfer(si);
    }
    case STATE_AWAITING_HEADER:
    {
        memcpy(&si->buf[si->accumulated], read_buf, read_size);
        si->accumulated += read_size;

        uint32_t remaining = PDU_HEADER_LEN - si->accumulated;

        if (remaining == 0) {
            if (unpack_header(&si->buf[0], PDU_HEADER_LEN, &si->header))
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
        }
        else
        {
            bus_sink_cb_res_t res = {
                .next_read = remaining,
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
            si->accumulated = 0;
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
        KINETIC_ASSERT(false);
    }
}

static void log_response_seq_id(int fd, int64_t seq_id) {
    #if KINETIC_LOGGER_LOG_SEQUENCE_ID
    struct timeval tv;
    gettimeofday(&tv, NULL);
    LOGF2("SEQ_ID response fd %d seq_id %lld %08lld.%08d",
        fd, (long long)seq_id,
        (long)tv.tv_sec, (long)tv.tv_usec);
    #else
    (void)seq_id;
    #endif
}

STATIC bus_unpack_cb_res_t unpack_cb(void *msg, void *socket_udata) {
    KineticSession * session = (KineticSession*)socket_udata;
    KINETIC_ASSERT(session);

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
        if (response->proto->has_commandbytes &&
            response->proto->commandbytes.data != NULL &&
            response->proto->commandbytes.len > 0)
        {
            response->command = KineticPDU_unpack_command(NULL,
                response->proto->commandbytes.len, response->proto->commandbytes.data);
        } else {
            response->command = NULL;
        }

        if (response->header.valueLength > 0)
        {
            memcpy(response->value, &si->buf[si->header.protobufLength], si->header.valueLength);
        }

        int64_t seq_id = BUS_NO_SEQ_ID;
        if (response->command != NULL &&
            response->command->header != NULL)
        {
            if (response->proto->has_authtype &&
                response->proto->authtype == COM__SEAGATE__KINETIC__PROTO__MESSAGE__AUTH_TYPE__UNSOLICITEDSTATUS
                && KineticSession_GetConnectionID(session) == 0)
            {
                /* Ignore the unsolicited status message on connect. */
                seq_id = BUS_NO_SEQ_ID;
            } else {
                seq_id = response->command->header->acksequence;
            }
            log_response_seq_id(session->socket, seq_id);
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

bool KineticBus_Init(KineticClient * client, KineticClientConfig * config)
{
    int log_level = config->logLevel;

    bus_config cfg = {
        .log_cb = log_cb,
        .log_level = (log_level <= 1) ? 0 : log_level,
        .sink_cb = sink_cb,
        .unpack_cb = unpack_cb,
        .unexpected_msg_cb = KineticController_HandleUnexpectedResponse,
        .bus_udata = NULL,
        .listener_count = config->readerThreads,
        .threadpool_cfg = {
            .max_threads = config->maxThreadpoolThreads,
        },
    };
    bus_result res;
    memset(&res, 0, sizeof(res));
    if (!Bus_Init(&cfg, &res)) {
        LOGF0("failed to init bus: %d", res.status);
        return false;
    }
    client->bus = res.bus;
    return true;
}

void KineticBus_Shutdown(KineticClient * const client)
{
    if (client) {
        Bus_Shutdown(client->bus);
        Bus_Free(client->bus);
        client->bus = NULL;
    }
}
