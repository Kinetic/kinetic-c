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

KineticStatus KineticOperation_GetStatus(const KineticOperation* const operation)
{
    KineticStatus status = KINETIC_STATUS_INVALID;

    if (operation != NULL &&
        operation->response != NULL &&
        operation->response->proto != NULL &&
        operation->response->proto->command != NULL &&
        operation->response->proto->command->status != NULL &&
        operation->response->proto->command->status->has_code != false &&
        operation->response->proto->command->status->code !=
        KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE) {
        switch (operation->response->proto->command->status->code) {
        case KINETIC_PROTO_STATUS_STATUS_CODE_SUCCESS:
            status = KINETIC_STATUS_SUCCESS;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_REMOTE_CONNECTION_ERROR:
            status = KINETIC_STATUS_CONNECTION_ERROR;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_SERVICE_BUSY:
            status = KINETIC_STATUS_DEVICE_BUSY;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_REQUEST:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_ATTEMPTED:
        case KINETIC_PROTO_STATUS_STATUS_CODE_HEADER_REQUIRED:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NO_SUCH_HMAC_ALGORITHM:
            status = KINETIC_STATUS_INVALID_REQUEST;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_MISMATCH:
        case KINETIC_PROTO_STATUS_STATUS_CODE_VERSION_FAILURE:
            status = KINETIC_STATUS_VERSION_FAILURE;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_DATA_ERROR:
        case KINETIC_PROTO_STATUS_STATUS_CODE_HMAC_FAILURE:
        case KINETIC_PROTO_STATUS_STATUS_CODE_PERM_DATA_ERROR:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_FOUND:
            status = KINETIC_STATUS_DATA_ERROR;
            break;

        case KINETIC_PROTO_STATUS_STATUS_CODE_INTERNAL_ERROR:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NOT_AUTHORIZED:
        case KINETIC_PROTO_STATUS_STATUS_CODE_EXPIRED:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NO_SPACE:
        case KINETIC_PROTO_STATUS_STATUS_CODE_NESTED_OPERATION_ERRORS:
            status = KINETIC_STATUS_OPERATION_FAILED;
            break;

        default:
        case KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE:
        case _KINETIC_PROTO_STATUS_STATUS_CODE_IS_INT_SIZE:
            status = KINETIC_STATUS_INVALID;
            break;
        }
    }
    return status;
}

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
                               const KineticKeyValue* metadata)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_PUT;
    operation->request->proto->command->header->has_messageType = true;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, metadata);

    operation->request->value = metadata->value;
}

void KineticOperation_BuildGet(KineticOperation* operation,
                               const KineticKeyValue* metadata)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_GET;
    operation->request->proto->command->header->has_messageType = true;

    operation->request->value = BYTE_ARRAY_NONE;
    if (metadata->value.data != NULL) {
        operation->response->value.data = metadata->value.data;
    }
    else {
        operation->response->value.data = operation->response->valueBuffer;
    }
    operation->response->value.len = PDU_VALUE_MAX_LEN;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, metadata);
}

void KineticOperation_BuildDelete(KineticOperation* operation,
                                  const KineticKeyValue* metadata)
{
    KineticOperation_ValidateOperation(operation);
    KineticConnection_IncrementSequence(operation->connection);

    operation->request->proto->command->header->messageType = KINETIC_PROTO_MESSAGE_TYPE_DELETE;
    operation->request->proto->command->header->has_messageType = true;
    operation->request->value = BYTE_ARRAY_NONE;
    operation->response->value = BYTE_ARRAY_NONE;

    KineticMessage_ConfigureKeyValue(&operation->request->protoData.message, metadata);
}
