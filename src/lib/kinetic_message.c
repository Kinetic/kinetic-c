/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
*/

#include "kinetic_message.h"
#include "kinetic_logger.h"

// e.g. CONFIG_FIELD_BYTE_BUFFER(key, message->keyValue, entry)
#define CONFIG_FIELD_BYTE_BUFFER(_name, _field, _entry) { \
    if ((_entry)->_name.array.data != NULL \
        && (_entry)->_name.array.len > 0 \
        && (_entry)->_name.bytesUsed > 0 \
        && (_entry)->_name.bytesUsed <= (_entry)->_name.array.len) \
    { \
        (_field)._name.data = (_entry)->_name.array.data; \
        (_field)._name.len = (_entry)->_name.bytesUsed; \
        (_field).has_ ## _name = true; \
    } \
    else { \
        (_field).has_ ## _name = false; \
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
    message->command.body->keyValue = &message->keyValue;

    // Set keyValue fields appropriately
    CONFIG_FIELD_BYTE_BUFFER(key,        message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(newVersion, message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(dbVersion,  message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(tag,        message->keyValue, entry);

    message->keyValue.has_force = (bool)((int)entry->force);
    if (message->keyValue.has_force) {
        message->keyValue.force = entry->force;
    }

    message->keyValue.has_algorithm = (bool)((int)entry->algorithm > 0);
    if (message->keyValue.has_algorithm) {
        message->keyValue.algorithm =
            Com_Seagate_Kinetic_Proto_Command_Algorithm_from_KineticAlgorithm(entry->algorithm);
    }
    message->keyValue.has_metadataOnly = entry->metadataOnly;
    if (message->keyValue.has_metadataOnly) {
        message->keyValue.metadataOnly = entry->metadataOnly;
    }

    message->keyValue.has_synchronization = (entry->synchronization > 0);
    if (message->keyValue.has_synchronization) {
        message->keyValue.synchronization =
            Com_Seagate_Kinetic_Proto_Command_Synchronization_from_KineticSynchronization(
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
    message->command.body->range->has_startKey = 
        (range->startKey.array.data != NULL);
    if (message->command.body->range->has_startKey) {
        message->command.body->range->startKey = (ProtobufCBinaryData) {
            .data = range->startKey.array.data,
            .len = range->startKey.bytesUsed,
        };
    }

    // Populate endKey, if supplied
    message->command.body->range->has_endKey = 
        (range->endKey.array.data != NULL);
    if (message->command.body->range->has_endKey) {
        message->command.body->range->endKey = (ProtobufCBinaryData) {
            .data = range->endKey.array.data,
            .len = range->endKey.bytesUsed,
        };
    }

    // Populate start/end key inclusive flags, if specified
    if (range->startKeyInclusive) {
        message->command.body->range->startKeyInclusive = range->startKeyInclusive;
    }
    message->command.body->range->has_startKeyInclusive = range->startKeyInclusive;
    if (range->endKeyInclusive) {
        message->command.body->range->endKeyInclusive = range->endKeyInclusive;
    }
    message->command.body->range->has_endKeyInclusive = range->endKeyInclusive;

    // Populate max keys to return
    message->command.body->range->maxReturned = range->maxReturned;
    message->command.body->range->has_maxReturned = true;

    // Populate reverse flag (return keys in reverse order)
    if (range->reverse) {
        message->command.body->range->reverse = range->reverse;
    }
    message->command.body->range->has_reverse = range->reverse;
}
