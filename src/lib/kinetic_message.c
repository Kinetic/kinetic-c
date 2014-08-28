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

*/

#include "kinetic_message.h"

void KineticMessage_Init(KineticMessage* const message)
{
    // Initialize protobuf fields and ssemble the message
    KINETIC_MESSAGE_INIT(message);
}

// e.g. CONFIG_FIELD_BYTE_ARRAY(key, message->keyValue, metadata)
#define CONFIG_FIELD_BYTE_ARRAY(_name, _field, _config) { \
    if (_config->_name.data != NULL && _config->_name.len > 0) { \
        _field._name = _config->_name; \
        _field.has_ ## _name = true; \
    } \
}

// #define CONFIG_OPTIONAL_FIELD_ENUM(name, field, message) { \
//     message->keyValue.(name) = ((int)metadata->algorithm > 0); \
//     if (message->keyValue.has_(name)) { \
//         message->keyValue.algorithm = algorithm; } }

void KineticMessage_ConfigureKeyValue(KineticMessage* const message,
    const Kinetic_KeyValue* metadata)
{
    assert(metadata != NULL);

    // Enable command body and keyValue fields by pointing at
    // pre-allocated elements in message
    message->command.body = &message->body;
    message->proto.command->body = &message->body;
    message->command.body->keyValue = &message->keyValue;
    message->proto.command->body->keyValue = &message->keyValue;

    // Set keyValue fields appropriately
    CONFIG_FIELD_BYTE_ARRAY(key, message->keyValue, metadata);
    CONFIG_FIELD_BYTE_ARRAY(newVersion, message->keyValue, metadata);
    CONFIG_FIELD_BYTE_ARRAY(dbVersion, message->keyValue, metadata);
    CONFIG_FIELD_BYTE_ARRAY(tag, message->keyValue, metadata);
    message->keyValue.has_algorithm = (bool)((int)metadata->algorithm > 0);
    if (message->keyValue.has_algorithm) {
        message->keyValue.algorithm = metadata->algorithm; }
    message->keyValue.has_metadataOnly = metadata->metadataOnly;
    if (message->keyValue.has_metadataOnly) {
        message->keyValue.metadataOnly = metadata->metadataOnly; }
}
