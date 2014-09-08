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
*
*/

#include "kinetic_operation.h"
#include "kinetic_connection.h"
#include "kinetic_message.h"
#include "kinetic_logger.h"

void KineticOperation_ValidateOperation(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->request->proto != NULL);
    assert(operation->request->proto->command != NULL);
    assert(operation->request->proto->command->header != NULL);
    assert(operation->response != NULL);
}

void KineticOperation_BuildNoop(KineticOperation* operation)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_NOOP;
    operation->request->proto->command->header->has_messageType = true;
}

void KineticOperation_BuildPut(KineticOperation* operation,
    const Kinetic_KeyValue* metadata,
    const ByteArray value)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_PUT;
    operation->request->proto->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, metadata);

    operation->request->value = value;
}

void KineticOperation_BuildGet(KineticOperation* operation,
    const Kinetic_KeyValue* metadata,
    const ByteArray value)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_GET;
    operation->request->proto->command->header->has_messageType = true;

    operation->request->value = BYTE_ARRAY_NONE;
    if (value.data != NULL)
    {
        operation->response->value = value;
    }
    else
    {
        operation->response->value.data = operation->response->valueBuffer;
    }

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, metadata);
}
