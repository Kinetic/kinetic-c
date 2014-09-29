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

*/

#include "kinetic_message.h"

void KineticMessage_Init(KineticMessage* const message)
{
    assert(message != NULL);
    KINETIC_MESSAGE_INIT(message);
}

// e.g. CONFIG_FIELD_BYTE_BUFFER(key, message->keyValue, entry)
#define CONFIG_FIELD_BYTE_BUFFER(_name, _field, _entry) { \
    if ((_entry)->_name.array.data != NULL && (_entry)->_name.array.len > 0) { \
        _field._name.data = (_entry)->_name.array.data; \
        _field._name.len = (_entry)->_name.array.len; \
        _field.has_ ## _name = true; \
    } \
}

void KineticMessage_ConfigureKeyValue(KineticMessage* const message,
                                      const KineticEntry* entry)
{
    assert(message != NULL);
    assert(entry != NULL);

    // Enable command body and keyValue fields by pointing at
    // pre-allocated elements in message
    message->command.body = &message->body;
    message->proto.command->body = &message->body;
    message->command.body->keyValue = &message->keyValue;
    message->proto.command->body->keyValue = &message->keyValue;

    // Set keyValue fields appropriately
    CONFIG_FIELD_BYTE_BUFFER(key, message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(newVersion, message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(dbVersion, message->keyValue, entry);
    CONFIG_FIELD_BYTE_BUFFER(tag, message->keyValue, entry);

    message->keyValue.has_force = (bool)((int)entry->force);
    if (message->keyValue.has_force) {
        message->keyValue.force = entry->force;
    }

    message->keyValue.has_algorithm = (bool)((int)entry->algorithm > 0);
    if (message->keyValue.has_algorithm) {
        message->keyValue.algorithm =
            KineticProto_Algorithm_from_KineticAlgorithm(entry->algorithm);
    }
    message->keyValue.has_metadataOnly = entry->metadataOnly;
    if (message->keyValue.has_metadataOnly) {
        message->keyValue.metadataOnly = entry->metadataOnly;
    }

    message->keyValue.has_synchronization = (entry->synchronization > 0);
    if (message->keyValue.has_synchronization) {
        message->keyValue.synchronization =
            KineticProto_Synchronization_from_KineticSynchronization(
                entry->synchronization);
    }
}
