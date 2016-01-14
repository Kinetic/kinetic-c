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

#include "listener_helper.h"
#include "listener_task.h"
#include "syscall.h"
#include "atomic.h"

#include <assert.h>

#ifdef TEST
uint8_t msg_buf[sizeof(uint8_t)];
#endif

listener_msg *ListenerHelper_GetFreeMsg(listener *l) {
    struct bus *b = l->bus;

    BUS_LOG_SNPRINTF(b, 4, LOG_LISTENER, b->udata, 128,
        "get_free_msg -- in use: %d", l->msgs_in_use);

    for (;;) {
        listener_msg *head = l->msg_freelist;
        if (head == NULL) {
            BUS_LOG(b, 3, LOG_LISTENER, "No free messages!", b->udata);
            return NULL;
        } else if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msg_freelist, head, head->next)) {
            for (;;) {
                int16_t miu = l->msgs_in_use;
                assert(miu < MAX_QUEUE_MESSAGES);

                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&l->msgs_in_use, miu, miu + 1)) {
                    BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 64,
                        "got free msg: %u", head->id);

                    /* Add counterpressure between the client and the listener.
                     * 10 * ((n >> 1) ** 2) microseconds */
                    int16_t delay = 10 * (miu >> 1) * (miu >> 1);
                    if (delay > 0) {
                        struct timespec ts = {
                            .tv_sec = 0,
                            .tv_nsec = 1000L * delay,
                        };
                        nanosleep(&ts, NULL);
                    }
                    BUS_ASSERT(b, b->udata, head->type == MSG_NONE);
                    memset(&head->u, 0, sizeof(head->u));
                    return head;
                }
            }
        }
    }
}

bool ListenerHelper_PushMessage(struct listener *l, listener_msg *msg, int *reply_fd) {
    struct bus *b = l->bus;
    BUS_ASSERT(b, b->udata, msg);

    #ifndef TEST
    uint8_t msg_buf[sizeof(uint8_t)];
    #endif
    msg_buf[0] = msg->id;

    if (reply_fd) { *reply_fd = msg->pipes[0]; }

    for (;;) {
        ssize_t wr = syscall_write(l->commit_pipe, msg_buf, sizeof(msg_buf));
        if (wr == sizeof(msg_buf)) {
            return true;  // committed
        } else {
            if (errno == EINTR) { /* signal interrupted; retry */
                errno = 0;
                continue;
            } else {
                BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                    "write_commit error, errno %d", errno);
                errno = 0;
                ListenerTask_ReleaseMsg(l, msg);
                return false;
            }
        }
    }
}

rx_info_t *ListenerHelper_GetFreeRXInfo(struct listener *l) {
    struct bus *b = l->bus;

    struct rx_info_t *head = l->rx_info_freelist;
    if (head == NULL) {
        BUS_LOG(b, 6, LOG_SENDER, "No rx_info cells left!", b->udata);
        return NULL;
    } else {
        l->rx_info_freelist = head->next;
        head->next = NULL;
        l->rx_info_in_use++;
        BUS_LOG(l->bus, 4, LOG_LISTENER, "reserving RX info", l->bus->udata);
        BUS_ASSERT(b, b->udata, head->state == RIS_INACTIVE);
        if (l->rx_info_max_used < head->id) {
            BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
                "rx_info_max_used <- %d", head->id);
            l->rx_info_max_used = head->id;
            BUS_ASSERT(b, b->udata, l->rx_info_max_used < MAX_PENDING_MESSAGES);
        }

        BUS_LOG_SNPRINTF(b, 5, LOG_LISTENER, b->udata, 128,
            "got free rx_info_t %d (%p)", head->id, (void *)head);
        BUS_ASSERT(b, b->udata, head == &l->rx_info[head->id]);
        return head;
    }
}

rx_info_t *ListenerHelper_FindInfoBySequenceID(listener *l,
        int fd, int64_t seq_id) {
    struct bus *b = l->bus;
    for (int i = 0; i <= l->rx_info_max_used; i++) {
        rx_info_t *info = &l->rx_info[i];

        switch (info->state) {
        case RIS_INACTIVE:
            break;            /* skip */
        case RIS_HOLD:
            BUS_LOG_SNPRINTF(b, 4, LOG_MEMORY, b->udata, 128,
                "find_info_by_sequence_id: info (%p) at +%d: <fd:%d, seq_id:%lld>",
                (void*)info, info->id, fd, (long long)seq_id);
            if (info->u.hold.fd == fd && info->u.hold.seq_id == seq_id) {
                return info;
            }
            break;
        case RIS_EXPECT:
        {
            struct boxed_msg *box = info->u.expect.box;
            BUS_LOG_SNPRINTF(b, 4, LOG_MEMORY, b->udata, 128,
                "find_info_by_sequence_id: info (%p) at +%d [s %d]: box is %p",
                (void*)info, info->id, info->u.expect.error, (void*)box);
            if (box != NULL && box->out_seq_id == seq_id && box->fd == fd) {
                return info;
            }
            break;
        }
        default:
            BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
                "match fail %d on line %d", info->state, __LINE__);
            BUS_ASSERT(b, b->udata, false);
        }
    }

    if (b->log_level > 5 || 0) {
        BUS_LOG_SNPRINTF(b, 0, LOG_LISTENER, b->udata, 64,
            "==== Could not find <fd:%d, seq_id:%lld>, dumping table ====\n",
            fd, (long long)seq_id);
        ListenerTask_DumpRXInfoTable(l);
    }
    /* Not found. Probably an unsolicited status message. */
    return NULL;
}
