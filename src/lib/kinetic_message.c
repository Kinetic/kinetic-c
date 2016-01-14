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

#include "kinetic_message.h"
#include "kinetic_logger.h"

// e.g. CONFIG_FIELD_BYTE_BUFFER(key, message->keyValue, entry)
#define CONFIG_FIELD_BYTE_BUFFER(_name, _proto_name, _field, _entry) { \
    if ((_entry)->_name.array.data != NULL \
        && (_entry)->_name.array.len > 0 \
        && (_entry)->_name.bytesUsed > 0 \
        && (_entry)->_name.bytesUsed <= (_entry)->_name.array.len) \
    { \
        (_field)._proto_name.data = (_entry)->_name.array.data; \
        (_field)._proto_name.len = (_entry)->_name.bytesUsed; \
        (_field).has_ ## _proto_name = true; \
    } \
    else { \
        (_field).has_ ## _proto_name = false; \
    } \
}

void KineticMessage_ConfigureKeyValue(KineticMessage* const message,
                                      const KineticEntry* entry)
{
    KINETIC_ASSERT(message != NULL);
    KINETIC_ASSERT(entry != NULL);

    // Enable command body and keyValue fields by pointing at
    // pre-allocated elements in message
    message->command.body = &message->body;
    message->command.body->keyvalue = &message->keyValue;

    // Set keyValue fields appropriately
    CONFIG_FIELD_BYTE_BUFFER(key,        key,        message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(newVersion, newversion, message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(dbVersion,  dbversion,  message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(tag,        tag,        message->keyValue, entry);

    message->keyValue.has_force = (bool)((int)entry->force);
    if (message->keyValue.has_force) {
        message->keyValue.force = entry->force;
    }

    message->keyValue.has_algorithm = (bool)((int)entry->algorithm > 0);
    if (message->keyValue.has_algorithm) {
        message->keyValue.algorithm =
            Com__Seagate__Kinetic__Proto__Command__Algorithm_from_KineticAlgorithm(entry->algorithm);
    }
    message->keyValue.has_metadataonly = entry->metadataOnly;
    if (message->keyValue.has_metadataonly) {
        message->keyValue.metadataonly = entry->metadataOnly;
    }

    message->keyValue.has_synchronization = (entry->synchronization > 0);
    if (message->keyValue.has_synchronization) {
        message->keyValue.synchronization =
            Com__Seagate__Kinetic__Proto__Command__Synchronization_from_KineticSynchronization(
                entry->synchronization);
    }
}


void KineticMessage_ConfigureKeyRange(KineticMessage* const message,
                                      const KineticKeyRange* range)
{
    KINETIC_ASSERT(message != NULL);
    KINETIC_ASSERT(range != NULL);
    KINETIC_ASSERT(range->startKey.array.data != NULL);
    KINETIC_ASSERT(range->startKey.array.len > 0);
    KINETIC_ASSERT(range->startKey.bytesUsed > 0);
    KINETIC_ASSERT(range->startKey.bytesUsed <= range->startKey.array.len);
    KINETIC_ASSERT(range->maxReturned > 0);

    // Enable command body and keyValue fields by pointing at
    // pre-allocated elements in message
    message->command.body = &message->body;
    message->command.body->range = &message->keyRange;

    // Populate startKey, if supplied
    message->command.body->range->has_startkey =
        (range->startKey.array.data != NULL);
    if (message->command.body->range->has_startkey) {
        message->command.body->range->startkey = (ProtobufCBinaryData) {
            .data = range->startKey.array.data,
            .len = range->startKey.bytesUsed,
        };
    }

    // Populate endKey, if supplied
    message->command.body->range->has_endkey =
        (range->endKey.array.data != NULL);
    if (message->command.body->range->has_endkey) {
        message->command.body->range->endkey = (ProtobufCBinaryData) {
            .data = range->endKey.array.data,
            .len = range->endKey.bytesUsed,
        };
    }

    // Populate start/end key inclusive flags, if specified
    if (range->startKeyInclusive) {
        message->command.body->range->startkeyinclusive = range->startKeyInclusive;
    }
    message->command.body->range->has_startkeyinclusive = range->startKeyInclusive;
    if (range->endKeyInclusive) {
        message->command.body->range->endkeyinclusive = range->endKeyInclusive;
    }
    message->command.body->range->has_endkeyinclusive = range->endKeyInclusive;

    // Populate max keys to return
    message->command.body->range->maxreturned = range->maxReturned;
    message->command.body->range->has_maxreturned = true;

    // Populate reverse flag (return keys in reverse order)
    if (range->reverse) {
        message->command.body->range->reverse = range->reverse;
    }
    message->command.body->range->has_reverse = range->reverse;
}
