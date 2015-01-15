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

#include "kinetic_admin_client.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_operation.h"
#include "kinetic_auth.h"

KineticClient * KineticAdminClient_Init(const char* log_file, int log_level)
{
    return KineticClient_Init(log_file, log_level);
}

void KineticAdminClient_Shutdown(KineticClient * const client)
{
    KineticClient_Shutdown(client);
}

KineticStatus KineticAdminClient_CreateConnection(KineticSession* const session,
    KineticClient * const client)
{
    return KineticClient_CreateConnection(session, client);
}

KineticStatus KineticAdminClient_DestroyConnection(KineticSession* const session)
{
    return KineticClient_DestroyConnection(session);
}


KineticStatus KineticAdminClient_SetErasePin(KineticSession const * const session,
    ByteArray pin)
{
    (void)pin;
    assert(session != NULL);
    assert(session->connection != NULL);
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticAdminClient_SecureErase(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsurePinSupplied(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildErase(operation, true);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_InstantErase(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsurePinSupplied(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildErase(operation, false);
    return KineticController_ExecuteOperation(operation, NULL);
}


KineticStatus KineticAdminClient_SetLockPin(KineticSession const * const session,
    ByteArray pin)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    (void)session;
    (void)pin;
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticAdminClient_LockDevice(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsurePinSupplied(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildLockUnlock(operation, true);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_UnlockDevice(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsurePinSupplied(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildLockUnlock(operation, false);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_GetLog(KineticSession const * const session,
                                   KineticDeviceInfo_Type type,
                                   KineticDeviceInfo** info,
                                   KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(info != NULL);

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildGetLog(operation, type, info);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticAdminClient_SetAcl(KineticSession const * const session,
    char const * const acl_path)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    (void)session;
    (void)acl_path;
    return KINETIC_STATUS_INVALID;
}

KineticStatus KineticAdminClient_SetClusterVersion(KineticSession const * const session,
    int64_t version)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    KineticOperation* operation = KineticOperation_Create(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildSetClusterVersion(operation, version);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_UpdateFirmware(KineticSession const * const session,
    char const * const fw_path)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    (void)session;
    (void)fw_path;
    return KINETIC_STATUS_INVALID;
}
