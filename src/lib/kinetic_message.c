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

void KineticMessage_ConfigureKeyValue(KineticMessage* const message,
    const char* newVersion, const char* key, const char* dbVersion, const char* tag)
{
    // Enable command body and keyValue fields by pointing at
    // pre-allocated elements in message
    message->command.body = &message->body;
    message->proto.command->body = &message->body;
    message->command.body->keyvalue = &message->keyValue;
    message->proto.command->body->keyvalue = &message->keyValue;

    // Set keyValue fields appropriately
    if (newVersion)
    {
        message->keyValue.has_newversion = true;
        message->keyValue.newversion.data = (uint8_t*)newVersion;
        message->keyValue.newversion.len = strlen(newVersion);
    }
    if (key)
    {
        message->keyValue.has_key = true;
        message->keyValue.key.data = (uint8_t*)key;
        message->keyValue.key.len = strlen(key);
    }
    if (dbVersion)
    {
        message->keyValue.has_dbversion = true;
        message->keyValue.dbversion.data = (uint8_t*)dbVersion;
        message->keyValue.dbversion.len = strlen(dbVersion);
    }
    if (tag)
    {
        message->keyValue.has_tag = true;
        message->keyValue.tag.data = (uint8_t*)tag;
        message->keyValue.tag.len = strlen(tag);
    }
}
